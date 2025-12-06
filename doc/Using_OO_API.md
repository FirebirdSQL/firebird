Firebird interfaces.

Firebird's OO API is based on use of interfaces. That interfaces, though looking in some aspects like OLE2 interfaces
(some of them have addRef() and release() methods) are non standard and have features, missing in other widely used
types of interfaces. First of all Firebird interfaces are **language independent** – that means that to define/use them
one need not use language specific constructions like _class_ in C++, interface may be defined using any language able
to call functions using C calling conventions and having concepts of array and pointer to procedure/function. Next
interfaces are **versioned** – i.e. we support different versions of same interface. Binary layout of interfaces is
designed to support that features very efficient (there is no need in additional virtual calls like in OLE2/COM with
it's **QueryInterface)****but it's not convenient for direct use from most languages. Therefore language-specific
wrappers should better be designed for different languages making use of API easier. Currently we have wrappers for C++
and Pascal, Java is coming soon. From end-user POV calls from C++ and Pascal have absolutely no difference, though some
additional language-specific features present in C++ (like ability to turn off automatic status check after API calls)
are missing in Pascal.**

Typically database API is used to access data stored in database. Firebird OO API certainly performs this task but in
addition it supports writing your own **plugins** – modules, making it possible to enhance Firebird capabilities
according to your needs. Therefore this document contains 2 big parts – accessing databases and writing plugins.
Certainly some interfaces (like status vector) are used in both parts of API, they will be discussed in data access part
and freely referenced later when discussing plugins. Therefore even if you plan to write some plugin you should better
start with the first part of this document. Moreover a lot of plugins need to access databases themselves and data
access API is typically needed for it.

<a name="result_box"></a> Firebird installation package contains a number of live samples of use of OO API – they are in
examples/interfaces (database access) and examples/dbcrypt (plugin performing fictitious database encryption)
directories. It's supposed that the reader is familiar with ISC API used in Firebird since interbase times.

This document does not pretend to be a full Firebird 3 documentation – it just describes new object oriented API, and a
reader should be familiar with main Firebird concepts, knowledge about ISC API is also much welcome. For example – when
describing how to work with services there is no explanation what is service and why is it needed, only description of
how to obtain [IService](#Service) interface and how to use it. Also pay attention that samples of code do not use a lot
of powerful features of C++. Not used reference counted pointers, not used other RAII holders, not used templates
(except one present in firebird public headers), etc. Primary goal is to make this text usable not only to C++ people
because our API is oriented to support not only C++ but other, more simple languages too.

# Creating database and attaching to existing database

First of all we need to get access to **IMaster** interface. IMaster is primary Firebird interface, required to access
all the rest of interfaces. Therefore there is a special way of accessing it – the only one needed to use OO API plain
function called fb_get_master_interface(). This function has no parameters and always succeeds. There is one and only
one instance of IMaster per Firebird client library, therefore one need not care about releasing memory, used by master
interface. A simplest way to access it from your program is to have appropriate global or static variable:

```cpp
static IMaster* master = fb_get_master_interface();
```

For a lot of methods, used in Firebird API, first parameter is **IStatus** interface. It's a logical replacement of
ISC_STATUS_ARRAY, but works separately with errors and warnings (not mixing them in same array), can contain unlimited
number of errors inside and (this will be important if you plan to implement IStatus yourself) always keeps strings,
referenced by it, inside interface. Typically you need at least one instance of IStatus to call other methods. You
obtain it from IMaster:

```cpp
IStatus* st = master->getStatus();
```

If method getStatus() fails for some reason (OOM for example) it returns NULL – obviously we can't use generic error
reporting method which is based on use of IStatus here.

Now we are going to deal with first interface, directly related to database calls. This is [IProvider](#Provider) –
interface called this way cause it's exactly that interface that must be implemented by any provider in Firebird.
Firebird client library also has it's own implementation of IProvider, which must be used to start any database
activity. To obtain it we call IMaster's method:

```cpp
IProvider* prov = master->getDispatcher();
```

When attaching to existing database or moreover creating new one it's often necessary to pass a lot of additional
parameters (logon/password, page size for new database, etc.) to API call. Having separate language-level parameters is
close to unreal – we will have to modify a call too often to add new parameters, and number of them will be very big no
matter of the fact that typically one needs to pass not too much of them. Therefore to pass additional parameters
special in-memory data structure, called _database parameters block_ (DPB) is used. Format of it is well defined, and
it's possible to build DPB byte after byte. But it's much easier to use special interface
[**IXpbBuilder**](#XpbBuilder), which simplifies creation of various parameters blocks. To obtain an instance of
IXpbBuilder you must know one more generic-use interface of firebird API – [**IUtil**](#Util). It's a kind of
placeholder for the calls that do not fit well in other places. So we do

```cpp
IUtil* utl = master->getUtilInterface();
IXpbBuilder* dpb = utl->getXpbBuilder(&status, IXpbBuilder::DPB, NULL, 0);
```

This creates empty parameters' block builder of DPB type. Now adding required parameter to it is trivial:

```cpp
dpb->insertInt(&status, isc_dpb_page_size, 4 * 1024);
```

will make firebird to create new database with pagesize equal to 4Kb and meaning of

```cpp
dpb->insertString(&status, isc_dpb_user_name, “sysdba”);
dpb->insertString(&status, isc_dpb_password, “masterkey”);
```

is (I hope) obvious.

<a name="Status Wrapper"></a> <u>The following is C++ specific:</u> We are almost ready to call createDatabase() method
of IProvider, but before it a few words about concept of **Status Wrapper** should be said. Status wrapper is not an
interface, it's very thin envelope for IStatus interface. It helps to customize behavior of C++ API (change a way how
errors, returned in IStatus interface, are processed). For the first time we recommend use of **ThrowStatusWrapper**,
which raises C++ exception each time an error is returned in IStatus.

```cpp
ThrowStatusWrapper status(st);
```
Now we may create new empty database:

```cpp
IAttachment* att = prov->createDatabase(&status, "fbtests.fdb", dpb->getBufferLength(&status), dpb->getBuffer(&status));
printf("Database fbtests.fdb created\n");
```
Pay attention that we do not check status after the call to createDatabase(), because in case of error C++ or Pascal
exception will be raised (therefore it's very good idea to have try/catch/except syntax in your program). We also use
two new functions from IXpbBuilder – getBufferLength() and getBuffer(), which extract data from interface in native
parameters block format. As you can see there is no need to check explicitly for status of functions, returning
intermediate results.

Detaching from just created database is trivial:

```cpp
att->detach(&status);
```

Now it remains to enclose all operators into _try_ block and write a handler in catch block. When using
ThrowStatusWrapper you should always catch defined in C++ API exception class FbException, in Pascal you must also work
with class FbException. Exception handler block in simplest case may look this way:
```cpp
catch (const FbException& error)
{
    char buf[256];
    utl->formatStatus(buf, sizeof(buf), error.getStatus());
    fprintf(stderr, "%s\n", buf);
}
```
Pay attention that here we use one more function from [IUtil](#Util) – formatStatus(). It returns in buffer text,
describing an error (warning), stored in IStatus parameter. To attach to existing database just use attachDatabase()
method of IProvider instead createDatabase(). All parameters are the same for both methods.

```cpp
att = prov->attachDatabase(&status, "fbtests.fdb", 0, NULL);
```

This sample is using no additional DPB parameters. Take into account that without logon/password any remote connection
will fail if no trusted authorization plugin is configured. Certainly login info may be also provided in environment (in
ISC_USER and ISC_PASSWORD variables) like it was before.

Our examples contain complete samples, dedicated except others to creating databases – 01.create.cpp and 01.create.pas.
When samples are present it will be very useful to build and try to run appropriate samples when reading this document.

# Working with transactions

Only creating empty databases is definitely not enough to work with RDBMS. We want to be able to create various objects
(like tables and so on) in database and insert data in that tables. Any operation within database is performed by
firebird under transaction control. Therefore first of all we must learn to start transaction. Here we do not discuss
distributed transactions (supported by [IDtc](#Dtc) interface) to avoid unneeded to most users overcomplication.
Starting of non-distributed transaction is very simple and done via attachment interface:
```cpp
ITransaction* tra = att->startTransaction(&status, 0, NULL);
```

In this sample default transaction parameters are used – no TPB is passed to startTransaction() method. If you need
non-default parameters you may create appropriate [IXpbBuilder](#XpbBuilder), add required items to it:

```cpp
IXpbBuilder* tpb = utl->getXpbBuilder(&status, IXpbBuilder::TPB, NULL, 0);
tpb->insertTag(&status, isc_tpb_read_committed);
```

and pass resulting TPB to startTransaction():

```cpp
ITransaction* tra = att->startTransaction(&status, tpb->getBufferLength(&status), tpb->getBuffer(&status));
```

Transaction interface is used as a parameter in a lot of other API calls but itself it does not perform any actions
except commit/rollback transaction, may be retaining:
```cpp
tra->commit(&status);
```

You may take a look at how to start and commit transaction in examples 01.create.cpp and 01.create.pas.

# Executing SQL operator without input parameters and returned rows

With started transaction we are ready to execute our first SQL operators. Used for it execute() method in
[IAttachment](#Attachment) is rather universal and may be also used to execute SQL operators with input and output
parameters (which is typical for EXECUTE PROCEDURE statement), but right now we will use the simple most form of it.
Both DDL and DML operators can be executed:

```cpp
att->execute(&status, tra, 0, "create table dates_table (d1 date)", SQL_DIALECT_V6, NULL, NULL, NULL, NULL);
tra->commitRetaining(&status);
att->execute(&status, tra, 0, "insert into dates_table values (CURRENT_DATE)", SQL_DIALECT_V6, NULL, NULL, NULL, NULL);
```

As you can see transaction interface is a required parameter for execute() method (must be NULL only if you execute
START TRANSACTION statement). Next follows length of SQL operator (may be zero causing use of C rules to determine
string length), text of operator and SQL dialect that should be used for it. The following for NULLs stand for metadata
descriptions and buffers of input parameters and output data. Complete description of this method is provided in
[IAttachment](#Attachment) interface.

You may take a look at how to start and commit transaction in examples 01.create.cpp and 01.create.pas.

# Executing SQL operator with input parameters

There are 2 ways to execute statement with input parameters. Choice of correct method depends upon do you need to
execute it more than once and do you know in advance format of parameters. When that format is known and statement is
needed to be run only once single call to IAttachment::execute() may be used. In other cases SQL statement should be
prepared first and after it executed, may be many times with different parameters. To prepare SQL statement for
execution use prepare() method of [IAttachment](#Attachment) interface:
```cpp
IStatement* stmt = att->prepare(&status, tra, 0, “UPDATE department SET budget = ? * budget + budget WHERE dept_no = ?”,
SQL_DIALECT_V6, IStatement::PREPARE_PREFETCH_METADATA);
```

If you are not going to use parameters description from firebird (i.e. you can provide that information yourself) please
use IStatement::PREPARE_PREFETCH_NONE instead PREPARE_PREFETCH_METADATA – this will save client/server traffic and
server resources a bit.

In ISC API XSQLDA is used to describe format of statement parameters. New API does not use XSQLDA – instead interface
IMessageMetadata is used. A set of input parameters (and also a row fetched from cursor) is described in firebird API in
same way and later called message. [IMessageMetadata](#MessageMetadata) is passed as a parameter to the methods
performing message exchange between your program and database engine. There are many ways to have an instance of
IMessageMetadata – one can:

- get it from [IStatement](#Statement),

- build using [IMetadataBuilder](#MetadataBuilder) interface,
- have your own implementation of this interface.

Getting metadata from prepared statement is very simple – method getInputMetadata() return interface describing input
message (i.e. statement parameters), interface returned by getOutputMetadata() describes output message (i.e. row in
selected data or values returned by procedure). In our case we can:

```cpp
IMessageMetadata* meta = stmt->getInputMetadata(&status);
```

Or we can build message metadata ourself. First of all we need builder interface for it:

```cpp
IMetadataBuilder* builder = master->getMetadataBuilder(&status, 2);
```

Second parameter is expected number of fields in the message, it can be changed later, i.e. that's just an optimization.

Now it's necessary to set individual fields characteristics in the builder. An absolute minimum is field types and
length for string fields:

```cpp
builder->setType(&status, 0, SQL_DOUBLE + 1);
builder->setType(&status, 1, SQL_TEXT + 1);
builder->setLength(&status, 1, 3);
```

New API is using old constants for SQL types, smallest bit as earlier stands for nullability. In some case it may also
make sense to set sub-type (for blobs), character set (for text fields) or scale (for numeric fields). And finally it's
time to get an instance of IMessageMetadata:
```cpp
IMessageMetadata* meta = builder->getMetadata(&status);
```

Here we do not discuss in details own implementation of IMessageMetadata. If one cares there is a sample
05.user_metadata.cpp.

So finally we have obtained (one or another way) an instance of metadata description of input parameters. But to work
with a message we also need buffer for it. Buffer size is one of main message metadata characteristics and it's returned
by method in IMessageMetadata:

```cpp
char* buffer = new char[meta->getMessageLength(&status)];
```
To deal with individual values inside buffer offset to them should be taken into an account. IMessageMetadata is aware
of offsets for all values in a message, using it we can create pointers to them:

```cpp
double* percent_inc = (double*) &buffer[meta->getOffset(&status, 0)];
char* dept_no = &buffer[meta->getOffset(&status, 1)];
```
Also let's do not forget to set to NOT NULL null flags:

```cpp
short* flag = (short*) &buffer[meta->getNullOffset(&status, 0)];
*flag = 0;
flag = (short*) &buffer[meta->getNullOffset(&status, 1)];
*flag = 0;
```

After finishing with offsets we are ready to execute statement with some parameters values:

```cpp
getInputValues(dept_no, percent_inc);
```
and may execute prepared statement:

```cpp
stmt->execute(&status, tra, meta, buffer, NULL, NULL);
```

Two more NULLs in the end of parameters stand for output message and is used typically for EXECUTE PROCEDURE statement.
If you do not need to get metadata from statement and plan to execute it only once you may choose a simpler way – use
method execute() in [IAttachment](#Attachment) interface:
```cpp
att->execute(&status, tra, 0,
    "UPDATE department SET budget = ? * budget + budget WHERE dept_no = ?",
    SQL_DIALECT_V6, meta, buffer, NULL, NULL);
```

In that case you do not need to use [IStatement](#Statement) at all.

An example how to execute UPDATE with parameters is present in 02.update.cpp, you will also see how raised in
trigger/procedure exception may be caught by C++ program.

# Opening cursor and fetching data from it

The only way to get rows of data, returned by SELECT operator, in OO API is to use [IResultSet](#ResultSet) interface.
This interface is returned by openCursor() method in both IAttachment and IStatement. openCursor() is in most aspects
alike execute() and a choice how to open cursor (using prepared statement or directly from attachment interface) is the
same. In the samples 03.select.cpp and 04.print_table.cpp both methods are used. Let's pay attention at one specific
openCursor() feature compared with execute() - one does not pass buffer for output message into openCursor(), it will be
passed later when data is fetched from cursor. This makes it possible to open cursor with unknown format of output
message (NULL is passed instead output metadata). Firebird is using in this case default message format which may be
requested from IResultSet interface:

```cpp
const char* sql = "select * from ..."; // some select statement
IResultSet* curs = att->openCursor(&status, tra, 0, sql, SQL_DIALECT_V6, NULL, NULL, NULL, NULL, 0);
IMessageMetadata* meta = curs->getMetadata(&status);
```

Later this metadata may be used to allocate buffer for data and parse fetched rows.

As an alternative one can first prepare statement, get metadata from prepared statement and after it open cursor. This
is preferred way if cursor is likely to be opened >1 times.

[IStatement](#Statement)* stmt = att->prepare(&status, tra, 0, sql, SQL_DIALECT_V6,
Istatement::PREPARE_PREFETCH_METADATA); [IMessageMetadata](#MessageMetadata)* meta = stmt->getOutputMetadata(&status);

[IResultSet](#ResultSet)* curs = stmt->openCursor(&status, tra, NULL, NULL, NULL, 0);

We have obtained (one or another way) an instance of metadata description of output fields (a row in a set). To work
with a message we also need buffer for it:

```cpp
unsigned char* buffer = new unsigned char[meta->getMessageLength(&status)];
```

IResultSet has a lot of various fetch methods but when cursor is not opened with SCROLL option only fetchNext() works,
i.e. one can navigate only forward record by record. In addition to errors and warnings in status fetchNext() returns
completion code, which may have values RESULT_OK (when buffer is filled with values for next row) or RESULT_NO_DATA
(when no more rows left in cursor). RESULT_NO_DATA is not error state, it's normal state after completion of the method,
but we know that data in cursor is over. If status wrapper, not throwing exception in case of error return is used, one
more value – RESULT_ERROR – may be returned, that means no data in buffer and error vector in status. Method fetchNext()
is usually called in a cycle:

```cpp
while (curs->fetchNext(&status, buffer) == IStatus::RESULT_OK)
{
    // row processing
}
```

What is done during row processing depends upon your needs. To access particular field field's offset should be used:

```cpp
unsigned char* field_N_ptr = buffer + meta->getOffset(&status, n);
```

where n is the number of a field in a message. That pointer should be casted to appropriate type, depending upon field
type. For example, for a VARCHAR field cast to struct vary should be used:

```cpp
vary* v_ptr = (vary*) (buffer + meta->getOffset(&status, n));
```

Now we may print the value of a field:

```cpp
printf(“field %s value is %*.*s\n”,
    meta->getField(&status, n),
    v_ptr->vary_length,
    v_ptr->vary_length,
    v_ptr->vary_string);
```

If you need maximum performance it will be good idea to cache needed metadata values like it's done in our samples
03.select.cpp and 04.print_table.cpp.

# Using FB_MESSAGE macro for static messages
Working with data using offsets is rather efficient but requires a lot of code to be written. In C++ this problem can be
solved using templates, but even compared with them the most convenient way to work with the message is to represent it
in native (for given language) way – structure in C/C++, record in Pascal, etc. Certainly this works only if format of a
message is known in advance. To help building such structures in C++ firebird contains special macro FB_MESSAGE.
FB_MESSAGE has 3 arguments – message (struct) name, type of status wrapper and list of fields. Usage of first and second
is obvious, list of fields contains pairs (field_type, field_name), where field_type is one of the following:

- FB_BIGINT
- FB_BLOB
- FB_BOOLEAN
- FB_CHAR(len)
- FB_DATE
- FB_DECFIXED(scale)
- FB_DECFLOAT16
- FB_DECFLOAT34
- FB_DOUBLE
- FB_FLOAT
- FB_INTEGER
- FB_INTL_CHAR(len, charSet)
- FB_INTL_VARCHAR(len, charSet)
- FB_SCALED_BIGINT(scale)
- FB_SCALED_INTEGER(scale)
- FB_SCALED_SMALLINT(scale)
- FB_SMALLINT
- FB_TIME
- FB_TIMESTAMP
- FB_VARCHAR(len)

In generated by preprocessor structure integer and float types are matched with appropriate C types, date and time –
with classes [FbDate](#FbDate) and [FbTime](#FbTime) (all mentioned here classes are in namespace Firebird), timestamp –
with class FbTimestamp, containing two public data members date and time of appropriate class, char - with struct
[FbChar](#FbChar) and varchar – with struct [FbVarChar](#FbVarChar). For each field preprocessor creates two data
members in the message – _name_ for field/parameter value and _nameNull_ for NULL indicator. Message constructor has 2
parameters – pointer to status wrapper and master interface:

```cpp
FB_MESSAGE(Output, ThrowStatusWrapper,
    (FB_SMALLINT, relationId)
    (FB_CHAR(31), relationName)
    (FB_VARCHAR(100), description)
) output(&status, master);
```

For static messages use of FB_MESSAGE is sooner of all the best choice – they can be at the same time easily passed to
execute, openCursor and fetch methods:

```cpp
rs = att->openCursor(&status, tra, 0, sqlText,
SQL_DIALECT_V6, NULL, NULL, output.getMetadata(), NULL, 0);
```

and used to work with values of individual fields:

```cpp
while (rs->fetchNext(&status, output.getData()) == IStatus::RESULT_OK)
{
    printf("%4d %31.31s %*.*s\n", output->relationId, output->relationName.str,
    output->descriptionNull ? 0 : output->description.length,
    output->descriptionNull ? 0 : output->description.length, output->description.str);
}
```
An example of using macro FB_MESSAGE to work with messages is in the sample 06.fb_message.cpp.
# Working with blobs
For blobs in message buffer firebird stores blob identifier – an 8-byte entity which should be aligned on 4-byte
boundary. Identifier has ISC_QUAD type. Interface [IAttachment](#Attachment) has 2 methods to work with blobs –
openBlob() and createBlob(), both returning interface [IBlob](#Blob) and having same set of parameters, but performing
somewhat contrary actions: openBlob() takes blob identifier from the message and prepares blob for reading but
createBlob() creates new blob, puts it's identifier into message and prepares blob for writing.

To work with blobs one must first of all include blob identifier into the message. If you get metadata from firebird
engine field of appropriate type will be already present. You just use it's offset (assuming variable blobFieldNumber
contains number of blob field) (and appropriate null offset to check for nulls or set null flag) to obtain pointer into
the message buffer:

```cpp
ISC_QUAD* blobPtr = (ISC_QUAD*) &buffer[metadata->getOffset(&status, blobFieldNumber)];
ISC_SHORT* blobNullPtr = (ISC_SHORT*) &buffer[metadata->getNullOffset(&status, blobFieldNumber)];
```

If you use static messages and FB_MESSAGE macro blob field is declared as having FB_BLOB type:
```cpp
FB_MESSAGE(Msg, ThrowStatusWrapper,
    (FB_BLOB, b)
) message(&status, master);

ISC_QUAD* blobPtr = &message->b;
ISC_SHORT* blobNullPtr = &message->bNull;
```
To create new blob invoke createBlob() method:
```cpp
IBlob* blob = att->createBlob(status, tra, blobPtr, 0, NULL);
```
Last two parameters are required only if you want to use blob filters or use stream blob, that's out of scope here.

Blob interface is ready to accept data into blob. Use putSegment() method to send data to engine:

```cpp
void* segmentData;
unsigned segmentLength;
while (userFunctionProvidingBlobData(&segmentData, &segmentLength))
blob->putSegment(&status, segmentLength, segmentData);
```

After sending some data to blob do not forget to close blob interface:
```cpp
blob->close(&status);
```

Make sure that null flag is not set (not required if you nullified all message buffer before creating blob):

```cpp
*blobNullPtr = 0;
```
and message, containing blob, may be used in insert or update statement. After execution of that statement new blob will
be stored in database. To read a blob begin with getting containing it's identifier message from firebird engine. This
may be done using fetch() or execute() methods. After it use openBlob() attachment's method:
```cpp
IBlob* blob = att->openBlob(status, tra, blobPtr, 0, NULL);
```

Blob interface is ready to provide blob data. Use getSegment() method to receive data from engine:

```cpp
char buffer[BUFSIZE];
unsigned actualLength;
for(;;)
{
    switch (blob->getSegment(&status, sizeof(buffer), buffer, &actualLength))
    {
        case IStatus::RESULT_OK:
            userFunctionAcceptingBlobData(buffer, actualLength, true);
            continue;
        case IStatus::RESULT_SEGMENT:
            userFunctionAcceptingBlobData(buffer, actualLength, false);
            continue;
        default:
            break;
    }
}
```

Last parameter in userFunctionAcceptingBlobData() is a flag that end of segment is reached – when getSegment() returns
RESULT_SEGMENT completion code that function is notified (by passing false as last parameter) that segment was not read
completely and continuation is expected at next call. After finishing with blob do not forget to close it:

```cpp
blob->close(&status);
```

<a name="Modifying data in a batch"></a>
# Modifying data in a batch
Since version 4 firebird supports batch execution of statements with input parameters – that means sending more than
single set of parameters when executing statement. [Batch](#Batch) interface is designed (first of all) in order to
satisfy JDBC requirements for prepared statement’s batch processing but has some serious differences:

- like all operations with data in firebird it’s oriented on messages, not single field;

- as an important extension out batch interface supports inline use of blobs (specially efficient when working with
  small blobs);

- execute() method returns not plain array of integers but special [BatchCompletionState](#BatchCompletionState)
  interface which can (depending upon batch creation parameters) contain both update records info and in addition to
  error flag detailed status vectors for messages that caused execution errors. [Batch](#Batch) (exactly like
  [ResultSet](#ResultSet)) may be created in 2 ways – using [Statement](#Statement) or [Attachment](#Attachment)
  interface, in both cases createBatch() method of appropriate interface is called. In second case text of SQL statement
  to be executed in a batch is passed directly to createBatch(). Tuning of batch operation is performed using [Batch
  parameters block](#Batch_PB) which has format more or less similar to DPB v.2 – tag in the beginning
  (IBatch::CURRENT_VERSION) followed by the set of wide clumplets: 1-byte tag, 4-byte length, length-byte value.
  Possible tags are [described](#Batch_PB) in batch interface. The simplest (and recommended) way to create parameters
  block for batch creation is to use appropriate [XpbBuilder](#XpbBuilder) interface:
```cpp
IXpbBuilder* pb = utl->getXpbBuilder(&status, IXpbBuilder::BATCH, NULL, 0);
pb->insertInt(&status, IBatch::RECORD_COUNTS, 1);
```
Use of such parameters block directs batch to account number of updated records on per-message basis. To create batch
interface with desired parameters pass parameters block to createBatch() call:
```cpp
IBatch* batch = att->createBatch(&status, tra, 0, sqlStmtText, SQL_DIALECT_V6, NULL,
pb->getBufferLength(&status), pb->getBuffer(&status));
```
In this sample batch interface is created with default format of messages cause NULL is passed instead input metadata
format. In order to proceed with created batch interface we need to know format of messages in it. It can be obtained
using getMetadata() method:
```cpp
IMessageMetadata* meta = batch->getMetadata(&status);
```
Certainly if you have passed your own format of messages to the batch you may simply use it. In the former text I
suppose that some function fillNextMessage(unsigned char* data, IMessageMetadata* metadata) is present and can fill
buffer ‘data’ according to passed format ‘metadata’. In order to work with messages we need a buffer for a data:

```cpp
unsigned char* data = new unsigned char[meta->getMessageLength(&status)];
```

Now we can add some messages full of data to the batch:

```cpp
fillNextMessage(data, meta);
batch->add(&status, 1, data);
fillNextMessage(data, meta);
batch->add(&status, 1, data);
```

An alternative way of working with messages (using FB_MESSAGE macro) is present in the sample of using batch interface
11.batch.cpp.

Finally batch should be executed:

```cpp
IBatchCompletionState* cs = batch->execute(&status, tra);
```

We requested accounting of the number of modified (inserted, updated or deleted) records per message. To print it we
must use [BatchCompletionState](#BatchCompletionState) interface. Determine total number of messages processed by batch
(it can be less than the number of messages passed to the batch if error happened and an option enabling multiple errors
during batch processing was not turned on):

```cpp
unsigned total = cs->getSize(&status);
```

Now print the state of each message:

```cpp
for (unsigned p = 0; p < total; ++p)
    printf(“Msg %u state %d\n”, p, cs->getState(&status, p));
```
When finished analyzing completion state don’t forget to dispose it:
```cpp
cs->dispose();
```

Full sample of printing contents of [BatchCompletionState](#BatchCompletionState) is in print_cs() function in sample
11.batch.cpp.

If for some reason you want to make batch buffers empty not executing it (i.e. prepare for new portion of messages to
process) use cancel() method:

```cpp
batch->cancel(&status);
```
Like the rest of our data access interfaces Batch has special method to close it:

```cpp
batch->close(&status);
```

Standard release() call may be used instead if one does not care about errors:

```cpp
batch->release();
```

Described methods help to implement all what one needs for JDBC-style prepared statement batch operations.

Notice: JDBC does not recommend to use too large batches, for example “ [Oracle recommends you to keep the batch sizes
in the general range of 50 to 100](https://docs.oracle.com/cd/E11882_01/java.112/e16548/oraperf.htm#JJDBC28753) ”.
Firebird supports bigger batches but it anyway has to limit maximum batch size – see
[TAG_BUFFER_BYTES_SIZE](#TAG_BUFFER_BYTES_SIZE). If total size of messages exceeds that limit isc_batch_too_big error is
returned. Pay attention that due to deep buffering of batch data when sending it over the wire you will not get this
error at once but only when buffers are flashed from client to server. This may happen in both add() and execute()
methods – execute() performs final flash. If you still wish to execute the batch with the messages that fit in a buffer
you may do it (when error was returned by execute() just repeat it). An actual number of processed messages will be
returned in [BatchCompletionState](#BatchCompletionState). An optimal batch size should be found for each particular
case but sooner of all having it >1000 hardly gives you serious performance increase.

One can add more than single message in one call to the batch. When doing it please remember – messages should be
appropriately aligned for this feature to work correctly. Required alignment and aligned size of the message should be
obtained from [MessageMetadata](#MessageMetadata) interface, for example:

```cpp
unsigned aligned = meta->getAlignedLength(&status);
```

Later that size will be useful when allocating an array of messages and working with it:
```cpp
unsigned char* data = new unsigned char[aligned * N]; // N is desired number of messages

for (int n = 0; n < N; ++n)
    fillNextMessage(&data[aligned * n], meta);

batch->add(&status, N, data);
```

After it batch may be executed or next portion of messages added to it.

Blobs in general are not compatible with batches – batch is efficient when one needs to pass a lot of small data to the
server in single step, blobs are treated as large objects and therefore in general it makes no sense to use them in
batches. But on practice it often happens that blobs are not too big – and in this case use of traditional blob API
(create blob, pass segments to the server, close blob, pass blobs ID in the message) kills performance, specially when
used over WAN. Therefore in firebird batch supports passing blobs to server inline, together with other messages. To use
that feature first of all [blob usage policy](#Batch_Blob_Policy) for a batch to be created should be set (as an option
in [parameters block](#Batch_PB)):

```cpp
pb->insertInt(&status, IBatch::BLOB_IDS, IBatch::BLOB_IDS_ENGINE);
```
In this example temporal blob IDs needed to keep a link between blob and a message where it’s used will be generated by
firebird engine – that’s the simplest and rather common usage. Imagine that the message is described as follows:

```cpp
FB_MESSAGE(Msg, ThrowStatusWrapper,
    (FB_VARCHAR(5), id)
    (FB_VARCHAR(10), name)
    (FB_BLOB, desc)
) project(&status, master);
```
In that case to send a message containing blob to the server one can do something like this:

```cpp
project->id = ++idCounter;
project->name.set(currentName);
batch->addBlob(&status, descriptionSize, descriptionText, &project->desc);
batch->add(&status, 1, project.getData());
```

If some blob happened to be big enough not to fit into your existing buffer you may instead reallocating buffer use
appendBlobData() method. It appends more data to last added blob.

```cpp
batch->addBlob(&status, descriptionSize, descriptionText, &project→desc, bpbLength, bpb);
```

After adding first part of blob get next portion of data into descriptionText, update descriptionSize and:

```cpp
batch->appendBlobData(&status, descriptionSize, descriptionText);
```

This may be done in a loop but take care not to overflow internal batch buffers – it’s size is controlled by
[BUFFER_BYTES_SIZE](#Batch_PB) option when creating batch interface but can’t exceed 256Mb (default is 16Mb). If you
need to process such big blob (for example on the background of a lot of small one – this can explain use of batch) just
use standard blob API and [registerBlob](#Batch::registerBlob) method of [Batch](#Batch) interface.

One more possible choice of blob policy is BLOB_IDS_USER. Usage at the first look does not change much – before calling
addBlob() correct and unique per batch execution ID should be placed to the memory referenced by last parameter.
Certainly same ID should be passed in the data message to the blob. Taking into an account that generation of blob IDs
by engine is very fast such policy may seem useless but imagine a case when you get blobs and other data in relatively
independent streams (blocks in a file for example) and some good IDs are already present in them. In such case use of
user-supplied blob IDs can greatly simplify your code. Please take into an account – unlike blobs created using regular
[createBlob](#Attachment) () blobs created by [Batch](#Batch) interface are by default stream, not segmented. Segmented
blobs provide nothing interesting compared with stream one and therefore not recommended to be used in new development,
we support that format only for backward compatibility reasons. If you really need segmented blobs this default may be
overridden by calling:

```cpp
batch->setDefaultBpb(&status,_ _bpbLength, bpb);
```

Certainly passed BPB may contain any other blob creation parameters too. As you may have already noticed you may also
pass BPB directly to addBlob() but if most of blobs you are going to add have same non-default format use of
setDefaultBpb() is slightly more efficient. Returning to segmented blobs – call to addBlob() will add first segment to
the blob, following calls to appendBlobData() will add more segments. Do not forget that segment size is limited to 64Kb
– 1, an attempt to pass more data in a single call with cause an error.

Next step when working with existing blob streams is use of addBlobStream() method. Using it one can add more than one
blob to the batch per single call. Blob stream is a sequence of blobs, each starts with blob header. Header should be
appropriately aligned - [Batch](#Batch) interface provides special call for this purpose:

```cpp
unsigned alignment = batch->getBlobAlignment(&status);
```

It’s supposed that all components of blob stream in a batch should be aligned at least at alignment boundary, including
size of stream portions passed to addBlobStream() which should be a multiple of this alignment. Header contains 3 fields
– 8-byte blob ID (must be non-zero), 4-byte total blob size and 4 byte BPB size. Total blob size includes BPB inside,
i.e. one will always find next blob in the stream in blob-size bytes after the header (taking into account the
alignment). BPB (if present, i.e. if BPB size is not zero) is placed right after the header. After BPB blob data goes,
it’s format depends upon blob type – stream or segmented. In case of stream blob it’s a plain sequence of bytes having
size blob-size – BPB-size. With segmented blob things are a bit more compicated: blob data is a set of segments where
each segment has the following format – 2-bytes size of segment (this should be aligned at IBatch::BLOB_SEGHDR_ALIGN
boundary) followed by stored in this 2 bytes number of bytes.

When big blob is added to the stream it’s size is not always known in advance. In order not to have too big buffer for
that blob (remember, size should be provided in blob header, before blob data) blob continuation record may be used. In
blob header you leave blob size at a value known when creating that header and add continuation record that has format
absolutely same as blob header, but here blob ID must be zero and BPB size must always be zero too. Typically you will
want to have one continuation record per addBlobStream() call.

<a name="Batch::registerBlob"></a> Last method used to work with blobs stands alone from the first three that pass blob
data inline with the rest of batch data – it’s needed to register in a batch ID of a blob created using standard blob
API. This may be unavoidable if one needs to pass to a batch really big blob. Do not use ID of such blob in batch
directly – that will cause invalid blob ID error during batch execution. Instead do:
```cpp
batch->registerBlob(&status, &realId, &msg->desc);
```

If blob policy makes firebird engine generate blob IDs this code is enough to correctly register existing blob in a
batch. In other cases you will have to assign correct (from batch POV) ID to msg->desc.

Almost all mentioned methods are used in 11.batch.cpp – please use it to see an alive sample of batching in firebird.

Two words about access to batches from ISC API - one can execute prepared ISC statement in batch mode. Main support for
it is presence of two new API functions, namely fb_get_transaction_interface & fb_get_statement_interface, which make it
possible to access appropriate interfaces identical to existing ISC handles. An example of it is present in
12.batch_isc.cpp.

# Working with events

Events interface was not completed in FB3, we expect to have something more interesting in next version. The minimum
existing support is as follows: [IAttachment](#Attachment) contains call queEvents() which performs almost same
functions as isc_que_events() call. Instead the pair of parameters _FPTR_EVENT_CALLBACK ast_ and _void* arg_, required
to invoke user code when event happens in firebird engine, callback interface IEventCallback is used. This is
traditional approach which helps to avoid non-safe casts from void* in user function. Another important difference is
that instead event identifier (a kind of handler) this function returns reference counted interface [IEvents](#Events)
having method cancel() used when waiting for event should be stopped. Unlike identifier which is automatically destroyed
when event arrives interface can not be automatically destroyed – in case when event is received right before canceling
interface call to cancel() would cause segfault when interface is already destroyed. Therefore interface
[IEvents](#Events) must be explicitly released after receiving an event. This may be done for example right before
queuing for an event next time:

```cpp
events->release();
events = NULL;
events = attachment->queEvents(&status, this, eveLen, eveBuffer);
```
Setting interface pointer to NULL is useful in case of exception during queEvents. In other aspects events handling did
not change compared with ISC API. Please use for additional details our sample 08.events.cpp.

# Using services

To begin to use services one should first of all connect to service manager. This is done using attachServiceManager()
method of [IProvider](#Provider). This method returns [IService](#Service) interface which is used later to talk to
service. To prepare SPB to attach to service manager one can use IXpbBuilder:

```cpp
IXpbBuilder* spb1 = utl->getXpbBuilder(&status, IXpbBuilder::SPB_ATTACH, NULL, 0);
spb1->insertString(&status, isc_spb_user_name, “sysdba”);
spb1->insertString(&status, isc_spb_password, “masterkey”);
```
and proceed with attach: [IService](#Service)* svc = prov->attachServiceManager(&status, “service_mgr”,
spb1->getBufferLength(&status), spb1->getBuffer(&status)); Using IService one can perform both available for services
actions – start services and query various information about started utilities and server in general. When querying
information one limitation takes place – formats of parameter blocks, used by query() method, in Firebird 3 are not
supported by IXpbBuilder. Support will be probably added in later versions, in Firebird 3 you will have to build and
analyze that blocks manually. Format of that blocks matches old format (used in ISC API) one to one.

To start service one should first of all create appropriate SPB:

IXpbBuilder* spb2 = utl->getXpbBuilder(&status, IXpbBuilder::SPB_START, NULL, 0);

and add required items to it. For example, to print encryption statistics for database employee the following should be
placed into SPB:

```cpp
spb2->insertTag(&status, isc_action_svc_db_stats);
spb2->insertString(&status, isc_spb_dbname, "employee");
spb2->insertInt(&status, isc_spb_options, isc_spb_sts_encryption);
```

After it service can be started using start() method of [IService](#Service) interface:

```cpp
svc->start(&status, spb2->getBufferLength(&status), spb2->getBuffer(&status));
```

Many started services (including mentioned here gstat) return text information during execution. To display it one
should query started service anout that information line by line. This is done by calling query() method of
[IService](#Service) interface with appropriate send and receive blocks of parameters. Send block may contain various
helper information (like timeout when querying service) or information to be passed to stdin of service utility or may
be empty in the simplest case. Receive block must contain list of tags you want to receive from service. For most of
utilities this is single isc_info_svc_line:

```cpp
const unsigned char receiveItems1[] = {isc_info_svc_line};
```

To query information one also needs a buffer for that information:

```cpp
unsigned char results[1024];
```

After that preliminary steps we are ready to query service in a loop (each line returned in a single call to query()):

```cpp
do
{
    svc->query(&status, 0, NULL, sizeof(receiveItems1), receiveItems1, sizeof(results), results);
} while (printInfo(results, sizeof(results)));
```

In this example we suppose that printInfo() function returns TRUE as long as service returns results block containing
next output line (i.e. till end of data stream from service). Format of results block varies from service to service,
and some services like gsec produce historical formats that are not trivial for parse – but this is out of our scope
here. A minimum working sample of printInfo() is present in example 09.service.cpp.

Same query method is used to retrieve information about server but in this case query function is not invoked in a loop,
i.e. buffer must be big enough to fit all information at once. This is not too hard – typically such calls do not return
much data. As in previous case begin with receive block placing required items in it – in our example it's
isc_info_svc_server_version: const unsigned char receiveItems2[] = {isc_info_svc_server_version};

Existing from previous call results buffer may be reused. No loop is needed here:

```cpp
svc->query(&status, 0, NULL, sizeof(receiveItems2), receiveItems2, sizeof(results), results);
printInfo(results, sizeof(results));
```

After finishing with services tasks do not forget to close an interface:

```cpp
svc->detach(&status);
```
IService interface also contains method cancel(). It’s needed to cancel wait in a query() and is used internally by
network server. Current implementation is restricted – IService::cancel() is not supported by remote redirector, only
engine provider supports it. That means one can use it only in embedded connections. Writing plugins.

To write a plugin means to implement some interfaces and place your implementation into dynamic library (.dll in windows
or .so in linux) later referenced as _plugin module_ or just _module_. In most cases single plugin is placed into
dynamic library but in common case multiple plugins may coexist in single dynamic library. One of that interfaces –
[IPluginModule](#PluginModule) – is module-wide (as more or less clear from it's name), others are per plugin. Also each
plugin module should contain special exported entrypoint firebird_plugin() which name is defined in include file
firebird/Interfaces.h as FB_PLUGIN_ENTRY_POINT.

In previous part of this text we were mostly describing how to use existing interfaces, here main attention will be paid
to implementing interfaces yourself. Certainly to do it one can and should use already existing interfaces both generic
needed for accessing firebird databases (already described) and some more interfaces specifically designed for plugins.

Following text is actively using example of database encryption plugin examples/dbcrypt/DbCrypt.cpp. It will be good
idea to compile this sample yourself and learn it when reading later.

# Implementation of plugin module

Plugins actively interact with special firebird component called _plugin manager_. In particular plugin manager should
be aware what plugin modules were loaded and must be notified if operating system tries to unload one of that modules
without explicit plugin manager command (this may happen first of all when using embedded access – when exit() is called
in a program or main firebird library _fbclient_ is unloaded). Primary task of IPluginModule interface is that
notification. First of all one must decide - how to detect that module is going to be unloaded? When dynamic library is
unloaded for some reason a lot of OS-dependent actions is performed and some of that actions may be used to detect this
fact in the program. When writing plugins distributed with firebird we always use invocation of destructor of global
variable. The big “plus” for this method is that it is OS independent (though something like atexit() function maybe
also used successfully). But use of destructor makes it possible to easily concentrate almost everything related with
unload detection in single class implementing at the same time [IPluginModule](#PluginModule) interface.

Minimum implementation looks as follows:
```cpp
class PluginModule : public IPluginModuleImpl<PluginModule, CheckStatusWrapper>
{
private:
    IPluginManager* pluginManager;

public:
    PluginModule()
        : pluginManager(NULL)
    { }

    ~PluginModule()
    {
        if (pluginManager)
        {
            pluginManager->unregisterModule(this);
            doClean();
        }
    }

    void registerMe(IPluginManager* m)
    {
        pluginManager = m;
        pluginManager->registerModule(this);
    }

    void doClean()
    {
        pluginManager = NULL;
    }
};
```

The only data member is plugin manager interface [IPluginManager](#PluginManager). It's passed to registerModule()
function and saved in private variable, at the same time module is registered in plugin manager by the call to
registerModule() method with own address as a single parameter. Variable _pluginManager_ not only stores pointer to
interface, at the same time it serves as a flag that module is registered. When destructor of registered module is
invoked it notifies plugin manager (yes, this is what for this class exists!) about unexpected unload by the call to
unregisterModule() passing pointer to itself. When plugin manager is going to unload module in regular way in first of
all calls doClean() method changing module state to unregistered and this avoiding call to unregisterModule() when OS
performs actual unload.

Implementing plugin's interface IPluginModule we met with first interface required to implement plugins –
[IPluginManager](#PluginManager). It will be actively used later, the rest of internals of this class will hardly be
required to you after copying it to your program. Just don't forget to declare global variable of this type and call
registerMe() function from FB_PLUGIN_ENTRY_POINT.

# Core interface of any plugin

Let's start implementing plugin itself. The type of main interface depends upon plugin type (which is obvious), but all
of them are based on common reference counted interface IPluginBase which performs common for all plugins (and very
simple) tasks. Each plugin has some (also reference counted) object which _owns_ this plugin. In order to perform smart
plugin lifetime management any plugin must be able to store that owner information and report it to plugin manager on
request. That means that each plugin must implement two trivial methods setOwner() and getOwner() contained in
IPluginBase interface. Type-dependent methods are certainly more interesting - they are discussed in interfaces
description part.

Let's take a look at typical part of any plugin implementation (here I specially use non-existent type SomePlugin):

```cpp
class MyPlugin : public ISomePluginImpl<MyPlugin, CheckStatusWrapper>
{
public:
    explicit MyPlugin(IPluginConfig* cnf) noexcept
        : config(cnf), refCounter(0), owner(NULL)
    {
        config->addRef();
    }
```

Constructor gets as parameter plugin configuration interface. If you are going to have you plugin configured in some way
it's good idea to save this interface in your plugin and use it later. This will let you use common for all firebird
configuration style letting users have familiar configuration and minimize code written. Certainly when saving any
reference counted interface it's better not forget to add reference to it. Also set reference counter to 0 and plugin
owner to NULL.

```cpp
    ~MyPlugin()
    {
        config->release();
    }
```

Destructor releases config interface. Pay attention – we do not change reference counter of our owner cause it owns us,
not we own it.

```cpp
    // IRefCounted implementation
    int release()
    {
        if (--refCounter == 0)
        {
            delete this;
            return 0;
        }
        return 1;
    }

    void addRef()
    {
        ++refCounter;
    }
```
Absolutely typical implementation of reference counted object.
```cpp
    // IPluginBase implementation
    void setOwner(IReferenceCounted* o)
    {
        owner = o;
    }

    IReferenceCounted* getOwner()
    {
        return owner;
    }
```
As it was promised implementation of IPluginBase is trivial.

```cpp
    // ISomePlugin implementation
    // … here go various methods required for particular plugin type
    private:
        IPluginConfig* config;
        std::atomic_int refCounter;
        IReferenceCounted* owner;
};
```

With this sample formal part of main plugin interface implementation is over. After adding type-specific methods (and
writing probably a lo-o-o-ot of code to make them useful) interface is ready.

# Plugin's factory

One more interface required for plugin to work is [IPluginFactory](#PluginFactory). Factory creates instances of plugin
and returns them to plugin manager. Factory typically looks this way:

```cpp
class Factory : public IPluginFactoryImpl<Factory, CheckStatusWrapper>
{
public:
    IPluginBase* createPlugin(CheckStatusWrapper* status, IPluginConfig* factoryParameter)
    {
        MyPlugin* p = new MyPlugin(factoryParameter);
        p->addRef();
        return p;
    }
};
```
Here attention should be payed to the fact that even in a case when code in a function may throw exceptions (operator
new may throw in a case when memory exhausted) one need not always manually define try/catch block – implementation of
firebird interfaces does this job for you, in implementation of IPluginFactory it's placed into template
IPluginFactoryImpl. Take into an account that default status wrappers perform meaning-full processing only for
FbException. But if you (that definitely makes sense if you work on some big project) define your own wrapper you can
handle any type of C++ exception and pass useful information about it from your plugin.

# Plugin module initialization entrypoint
When plugin manager loads plugin module it invokes module initializing routine – the only exported from plugin function
FB_PLUGIN_ENTRY_POINT. To wrote it's code one will need two global variables – plugin module and plugin factory. In our
case that is:
```cpp
PluginModule module;
Factory factory;
```

If you module contains more than one plugin you will need a factory per each plugin.

For FB_PLUGIN_ENTRY_POINT we should not forget that it should be exported from plugin module, and it requires taking
into an account some OS specifics. We do it using macro FB_DLL_EXPORT defined in examples/interfaces/ifaceExamples.h. If
you are sure you write plugin only for some specific OS you can make this place a bit simpler. In minimum case the
function should register module and all factories in plugin manager:

```cpp
extern "C" FB_DLL_EXPORT void FB_PLUGIN_ENTRY_POINT(IMaster* master)

{
    IPluginManager* pluginManager = master->getPluginManager();
    module.registerMe(pluginManager);
    pluginManager->registerPluginFactory(IPluginManager::TYPE_DB_CRYPT, "fbSampleDbCrypt", &factory);
}
```

First of all we call written by us not long ago PluginModule::registerMe() function which will saves IPluginManager for
future use and registers our plugin module. Next time to register factory (or factories in case of multiple plugins per
module) takes place. We must pass correct plugin type (valid types are enumerated in interface IPluginManager) and a
name under which plugin will be registered. In simple case it should match with the name of dynamic library with plugin
module. Following last rule will help you avoid configuring your plugin manually in plugins.conf.

Pay attention - unlike applications plugins should not use fb_get_master_interface() to obtain IMaster. Instance, passed
to FB_PLUGIN_ENTRY_POINT, should be used instead. If you ever need master interface in your plugin take care about
saving it in this function.

Interfaces: from A to Z In this glossary we do not list interfaces that are not actively used (like IRequest, needed
first of all to support legacy ISC API requests). Same reference may be made for a number of methods (like
compileRequest() in IAttachment). For interfaces / methods, having direct analogue in old API, that analogue is
provided.

# Generic interfaces

<a name="Attachment"></a> Attachment interface – replaces isc_db_handle:
- void getInfo(StatusType* status, unsigned itemsLength, const unsigned char* items, unsigned bufferLength, unsigned
  char* buffer) – replaces isc_database_info().
- ITransaction* startTransaction(StatusType* status, unsigned tpbLength, const unsigned char* tpb) – partially replaces
  isc_start_multiple(), to start >1 transaction [distributed transactions coordinator](#Dtc) should be used, also
  possible to [join](#Transaction) 2 transactions into single distributed transaction.
- ITransaction* reconnectTransaction(StatusType* status, unsigned length, const unsigned char* id) – makes it possible
  to connect to a transaction in limbo. Id parameter contains transaction number in network format of given length.
- IRequest* compileRequest(StatusType* status, unsigned blrLength, const unsigned char* blr) – support of ISC API.
- void transactRequest(StatusType* status, ITransaction* transaction, unsigned blrLength, const unsigned char* blr,
  unsigned inMsgLength, const unsigned char* inMsg, unsigned outMsgLength, unsigned char* outMsg) – support of ISC API.
- IBlob* createBlob(StatusType* status, ITransaction* transaction, ISC_QUAD* id, unsigned bpbLength, const unsigned
  char* bpb) – creates new blob, stores it's identifier in id, replaces isc_create_blob2().
- IBlob* openBlob(StatusType* status, ITransaction* transaction, ISC_QUAD* id, unsigned bpbLength, const unsigned char*
  bpb) – opens existing blob, replaces isc_open_blob2().
- int getSlice(StatusType* status, ITransaction* transaction, ISC_QUAD* id, unsigned sdlLength, const unsigned char*
  sdl, unsigned paramLength, const unsigned char* param, int sliceLength, unsigned char* slice) - support of ISC API.
- void putSlice(StatusType* status, ITransaction* transaction, ISC_QUAD* id, unsigned sdlLength, const unsigned char*
  sdl, unsigned paramLength, const unsigned char* param, int sliceLength, unsigned char* slice) - support of ISC API.
- void executeDyn(StatusType* status, ITransaction* transaction, unsigned length, const unsigned char* dyn) - support of
  ISC API.
- IStatement* prepare(StatusType* status, ITransaction* tra, unsigned stmtLength, const char* sqlStmt, unsigned dialect,
  unsigned flags) – replaces isc_dsql_prepare(). Additional parameter flags makes it possible to control what
  information will be preloaded from engine at once (i.e. in single network packet for remote operation).
- ITransaction* execute(StatusType* status, ITransaction* transaction, unsigned stmtLength, const char* sqlStmt,
  unsigned dialect, IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata, void* outBuffer) –
  executes any SQL statement except returning multiple rows of data. Partial analogue of isc_dsql_execute2() - in and
  out XSLQDAs replaced with input and output messages with appropriate buffers.
- IResultSet* openCursor(StatusType* status, ITransaction* transaction, unsigned stmtLength, const char* sqlStmt,
  unsigned dialect, IMessageMetadata* inMetadata, void* inBuffer, IMessageMetadata* outMetadata, const char* cursorName,
  unsigned cursorFlags) – executes SQL statement potentially returning multiple rows of data. Returns
  [ResultSet](#ResultSet) interface which should be used to fetch that data. Format of output data is defined by
  outMetadata parameter, leaving it NULL default format may be used. Parameter cursorName specifies name of opened
  cursor (analogue of isc_dsql_set_cursor_name()). Parameter cursorFlags is needed to open bidirectional cursor setting
  it's value to Istatement::CURSOR_TYPE_SCROLLABLE.
- IBatch* createBatch(StatusType* status, ITransaction* transaction, unsigned stmtLength, const char* sqlStmt, unsigned
  dialect, IMessageMetadata* inMetadata, unsigned parLength, const unsigned char* par) – prepares sqlStmt and creates
  [Batch](#Batch) interface ready to accept multiple sets of input parameters in inMetadata format. Leaving inMetadata
  NULL makes batch use default format for sqlStmt. Parameters block may be passed to createBatch() making it possible to
  adjust batch behavior.
- IEvents* queEvents(StatusType* status, IEventCallback* callback, unsigned length, const unsigned char* events) –
  replaces isc_que_events() call. Instead callback function with void* parameter callback interface is used.
- void cancelOperation(StatusType* status, int option) – replaces fb_cancel_operation().
- void ping(StatusType* status) – check connection status. If test fails the only operation possible with attachment is
  to close it.
- void detach(StatusType* status) – replaces isc_detach_database(). On success releases interface.
- void dropDatabase(StatusType* status) - replaces isc_drop_database(). On success releases interface.

<a name="Batch"></a> Batch interface – makes it possible to process multiple sets of parameters in single statement
execution.

- void add(StatusType* status, unsigned count, const void* inBuffer) – adds count messages from inBuffer to the batch.
  Total size of messages that can be added to the batch is limited by TAG_BUFFER_BYTES_SIZE [parameter](#Batch_PB) of
  batch creation.
- void addBlob(StatusType* status, unsigned length, const void* inBuffer, ISC_QUAD* blobId, unsigned bpbLength, const
  unsigned char* bpb) – adds single blob having length bytes from inBuffer to the batch, blob identifier is located at
  blobId address. If blob should be created with non-default parameters BPB may be passed (format matches one used in
  [Attachment](#Attachment)::createBlob).Total size of inline blobs that can be added to the batch (including optional
  BPBs, blob headers, segment sizes and taking into an accoount alignment) is limited by TAG_BUFFER_BYTES_SIZE
  [parameter](#Batch_PB) of batch creation (affects all blob-oriented methods except registerBlob()).
- void appendBlobData(StatusType* status, unsigned length, const void* inBuffer) – extend last added blob: append length
  bytes taken from inBuffer address to it.
- void addBlobStream(StatusType* status, unsigned length, const void* inBuffer) – adds blob data (this can be multiple
  objects or part of single blob) to the batch. Header of each blob in the stream is aligned at getBlobAlignment()
  boundary and contains 3 fields: first - 8-bytes blob identifier (in ISC_QUAD format), second - 4-bytes length of blob,
  third – 4-bytes length of BPB. Blob header should not cross boundaries of buffer in this function call. BPB data is
  placed right after header, blob data goes next. Length of blob includes BPB (if it present). All data may be
  distributed between multiple addBlobStream() calls. Blob data in turn may be structured in case of segmented blob, see
  chapter “ [Modifying data in a batch](#Modifying data in a batch) ”.
- void registerBlob(StatusType* status, const ISC_QUAD* existingBlob, ISC_QUAD* blobId) – makes it possible to use in
  batch blobs added using standard [Blob](#Blob) interface. This function contains 2 ISC_QUAD* parameters, it’s
  important not to mix them – second parameter (existingBlob) is a pointer to blob identifier, already added out of
  batch scope, third (blobId) points to blob identifier that will be placed in a message in this batch.
- [IBatchCompletionState](#BatchCompletionState)* execute(StatusType* status, ITransaction* transaction) – execute batch
  with parameters passed to it in the messages. If parameter MULTIERROR is not set in [parameters block](#Batch_PB) when
  creating the batch execution will be stopped after first error, in MULTIERROR mode an unlimited number of errors can
  happen, after an error execution is continued from the next message. This function returns BatchCompletionState
  interface that contains all requested nformation about the results of batch execution.
- void cancel(StatusType* status) – clear messages and blobs buffers, return batch to a state it had right after
  creation. Notice – being reference counted interface batch does not contain any special function to close it, please
  use release() for this purposes.
- unsigned getBlobAlignment(StatusType* status) – returns required alignment for the data placed into the buffer of
  addBlobStream().
- IMessageMetadata* getMetadata(StatusType* status) – return format of metadata used in batch’s messages.
- void setDefaultBpb(StatusType* status, unsigned parLength, const unsigned char* par) – sets BPB which will be used for
  all blobs missing non-default BPB. Must be called before adding any message or blob to batch.
- void getInfo(StatusType* status, unsigned itemsLength, const unsigned char* items, unsigned bufferLength, unsigned
  char* buffer) – requests information about batch.

<a name="Batch_PB"></a> Tag for parameters block: VERSION1 Tags for clumplets in parameters block:

- TAG_MULTIERROR (0/1) – can have >1 message with errors TAG_RECORD_COUNTS (0/1) - per-message modified records accounting
- <a name="TAG_BUFFER_BYTES_SIZE"></a> TAG_BUFFER_BYTES_SIZE (integer) - maximum possible buffer size on server (default
16Mb, maximum 256Mb) TAG_BLOB_POLICY - [policy](#Batch_Blob_Policy) used to store blobs
- TAG_DETAILED_ERRORS (integer) - how many vectors with detailed error info are stored in completion state (default 64,
maximum 256)

<a name="Batch_Blob_Policy"></a> Policies used to store blobs:

- BLOB_NONE – inline blobs can't be used (registerBlob() works anyway)
- BLOB_ID_ENGINE - blobs are added one by one, IDs are generated by firebird engine
- BLOB_ID_USER - blobs are added one by one, IDs are generated by user
- BLOB_STREAM - blobs are added in a stream

Items accepted in getInfo() call:
- INF_BUFFER_BYTES_SIZE – actual m aximum possible buffer size (one set by TAG_BUFFER_BYTES_SIZE)
- INF_DATA_BYTES_SIZE - already added messages size
- INF_BLOBS_BYTES_SIZE - already added blobs size
- INF_BLOB_ALIGNMENT - required alignment for the BLOB data (duplicates getBlobAlignment)

<a name="BatchCompletionState"></a> BatchCompletionState – disposable interface, always returned by execute() method of
[Batch](#Batch) interface. It contains more or less (depending upon parameters passed when [Batch](#Batch) was created)
detailed information about the results of batch execution.
- uint getSize(StatusType* status) – returns the total number of processed messages.
- int getState(StatusType* status, uint pos) – returns the result of execution of message number ‘pos’. On any error
  with the message this is EXECUTE_FAILED constant, value returned on success depends upon presence of RECORD_COUNTS
  [parameter](#Batch_PB) of batch creation. When it present and has non-zero value number of records inserted, updated
  or deleted during particular message processing is returned, else SUCCESS_NO_INFO constant is returned.
- uint findError(StatusType* status, uint pos) – finds next (starting with pos) message which processing caused an
  error. When such message is missing NO_MORE_ERRORS constant is returned. Number of status vectors, returned in this
  interface, is limited by the value of DETAILED_ERRORS [parameter](#Batch_PB) of batch creation.
- void getStatus(StatusType* status, IStatus* to, uint pos) – returns detailed information (full status vector) about an
  error that took place when processing ‘pos’ message. In order to distinguish between errors (in
  [Batch](#Batch)::execute() or in [BatchCompletionState](#BatchCompletionState)::getStatus()) that status is returned
  in separate ‘to’ parameter unlike errors in this call that are placed into ‘status’ parameter.

Special values returned by getState():

- EXECUTE_FAILED - error happened when processing this message
- SUCCESS_NO_INFO - record update info was not collected

Special value returned by findError():

- NO_MORE_ERRORS – no more messages with errors in this batch

<a name="Blob"></a> Blob interface – replaces isc_blob_handle:
- void getInfo(StatusType* status, unsigned itemsLength, const unsigned char* items, unsigned bufferLength, unsigned
  char* buffer) – replaces isc_blob_info().
- int getSegment(StatusType* status, unsigned bufferLength, void* buffer, unsigned* segmentLength) – replaces
  isc_get_segment(). Unlike it never returns isc_segstr_eof and isc_segment errors (that are actually not errors),
  instead returns [completion codes](#Completion codes) IStatus::RESULT_NO_DATA and IStatus::RESULT_SEGMENT, normal
  return is IStatus::RESULT_OK.
- void putSegment(StatusType* status, unsigned length, const void* buffer) – replaces isc_put_segment().
- void cancel(StatusType* status) – replaces isc_cancel_blob(). On success releases interface.
- void close(StatusType* status) – replaces isc_close_blob(). On success releases interface.
- int seek(StatusType* status, int mode, int offset) – replaces isc_seek_blob().

<a name="Config"></a> Config interface – generic configuration file interface:
- IConfigEntry* find(StatusType* status, const char* name) – find entry by name.
- IConfigEntry* findValue(StatusType* status, const char* name, const char* value) – find entry by name and value.
- IConfigEntry* findPos(StatusType* status, const char* name, unsigned pos) – find entry by name and position. If
  configuration file contains lines:
  ```
  Db=DBA
  Db=DBB
  Db=DBC
  ```
call to findPos(status, “Db”, 2) will return entry with value DBB.

<a name="ConfigManager"></a> ConfigManager interface – generic interface to access various configuration objects:

- const char* getDirectory(unsigned code) – returns location of appropriate directory in current firebird instance. See
  codes for this call a [few lines later](#Directory codes).
- IFirebirdConf* getFirebirdConf() - returns interface to access default configuration values (from firebird.conf).
- IFirebirdConf* getDatabaseConf(const char* dbName) - returns interface to access db-specific configuration (takes into
  an account firebird.conf and appropriate part of databases.conf).
- IConfig* getPluginConfig(const char* configuredPlugin) – returns interface to access named plugin configuration.
- const char* getInstallDirectory() - returns directory where firebird is installed.
- const char* getRootDirectory() - returns root directory of current instance, in single-instance case usually matches
  install directory.
- const char* getDefaultSecurityDb() - returns default (i.e. not taking into an account configuration files) pathname of
  security database, used first of all internally to enable correct access to security database on multi-provider server
  with zero configuration.

<a name="Directory codes"></a> Directory codes:

- DIR_BIN – bin (utilities like isql, gbak, gstat)
- DIR_SBIN – sbin (fbguard and firebird server)
- DIR_CONF – configuration files (firebird.conf, databases.conf, plugins.conf)
- DIR_LIB – lib (fbclient, ib_util)
- DIR_INC – include (ibase.h, firebird/Interfaces.h)
- DIR_DOC - documentation
- DIR_UDF – UDF (ib_udf, fbudf)
- DIR_SAMPLE - samples
- DIR_SAMPLEDB – samples database (employee.fdb)
- DIR_INTL – international libraries (fbintl)
- DIR_MISC – miscellaneous files (like uninstall manifest and something else)
- DIR_SECDB – where security database is stored (securityN.fdb)
- DIR_MSG – where messages file is stored (firebird.msg)
- DIR_LOG – where log file is stored (firebird.log)
- DIR_GUARD – where guardian lock is stored (fb_guard)
- DIR_PLUGINS – plugins directory ([lib]Engine14.{dll|so})

<a name="ConfigEntry"></a> ConfigEntry interface – represents an entry (Key = Values with probably sub-entries) in
firebird configuration file:

- const char* getName() - returns key name.
- const char* getValue() - returns value as character string.
- ISC_INT64 getIntValue() - treats value as integer and returns it.
- FB_BOOLEAN getBoolValue() - treats value as boolean and returns it.
- IConfig* getSubConfig(StatusType* status) – treats sub-entries as separate configuration file and returns Config
  interface for it.

<a name="DecFloat16"></a> <a name="DecFloat34"></a> DecFloat16 / DecFloat34 – interfaces that help to work with DECFLOAT
(16 & 34 respectively) datatypes. They have almost same set of methods with FB_DEC16 parameter replaced by FB_DEC34 for
DecFloat34:

- void toBcd(const FB_DEC16* from, int* sign, unsigned char* bcd, int* exp) – convert decimal float value to BCD.
- void toString(StatusType* status, const FB_DEC16* from, unsigned bufferLength, char* buffer) – convert decimal float
  value to string.
- void fromBcd(int sign, const unsigned char* bcd, int exp, FB_DEC16* to) – make decimal float value from BCD.
- void fromString(StatusType* status, const char* from, FB_DEC16* to) – make decimal float value from string.

<a name="Dtc"></a> Dtc interface – distributed transactions coordinator. Used to start distributed (working with 2 or
more attachments) transaction. Unlike pre-FB3 approach where distributed transaction must be started in this way from
the most beginning FB3's distributed transactions coordinator makes it also possible to join already started
transactions into single distributed transaction.

- ITransaction* join(StatusType* status, ITransaction* one, ITransaction* two) – joins 2 independent transactions into
  distributed transaction. On success both transactions passed to join() are released and pointers to them should not be
  used any more.
- IDtcStart* startBuilder(StatusType* status) – returns [DtcStart](#DtcStart) interface.

<a name="DtcStart"></a> DtcStart interface – replaces array of struct TEB (passed to isc_start_multiple() in ISC API).
This interface accumulates attachments (and probably appropriate TPBs) for which dustributed transaction should be
started.

- void addAttachment(StatusType* status, IAttachment* att) – adds attachment, transaction for it will be started with
  default TPB.
- void addWithTpb(StatusType* status, IAttachment* att, unsigned length, const unsigned char* tpb) - adds attachment and
  TPB which will be used to start transaction for this attachment.
- ITransaction* start(StatusType* status) – start distributed transaction for accumulated attachments. On successful
  return DtcStart interface is disposed automatically.

<a name="EventCallback"></a> EventCallback interface – replaces callback function used in isc_que_events() call. Should
be implemented by user to monitor events with IAttachment::queEvents() method.

- void eventCallbackFunction(unsigned length, const unsigned char* events) – is called each time event happens.

<a name="Events"></a> Events interface – replaces event identifier when working with events monitoring.

- void cancel(StatusType* status) - cancels events monitoring started by IAttachment::queEvents().

FirebirdConf interface – access to main firebird configuration. Used for both default configuration, set by
firebird.conf, and per-database configuration, adjusted by databases.conf. In order to speed up access to configuration
values calls accessing actual values use integer key instead symbolic parameter name. Key is stable during server run
(i.e. plugin can get it once and than use to get configuration value for different databases).

- unsigned getKey(const char* name) – get key for parameter name. ~0 (all bits are 1) is returned in a case when there
  is no such parameter.
- ISC_INT64 asInteger(unsigned key) – return value of integer parameter.
- const char* asString(unsigned key) - return value of string parameter.
- FB_BOOLEAN asBoolean(unsigned key) - return value of boolean parameter. Standard abbreviations (1/true/t/yes/y) are
  treated as “true”, all other cases – false.
- unsigned getVersion(StatusType* status) – get version of configuration manager associated with this interface.
  Different versions of configuration manager may coexist on same server for example when old DB engine is in use on
  modern server. Pay attention – keys (see getKey()) from different versions do not match and when used inappropriately
  will always return 0/nullptr/false.

<a name="Int128"></a> Int128 interfaces that help to work with 128-bit integers (used as base type for numeric and
decimal with precision > 18).

- void toString(StatusType* status, const FB_I128* from, int scale, unsigned bufferLength, char* buffer) – convert
  128-bit integer value to string taking scale into an account.
- void fromString(StatusType* status, int scale, const char* from, FB_I128* to) – make 128-bit integer value from string
  taking scale into an account.

<a name="Master"></a> Master interface – main interface from which start all operations with firebird API.

- IStatus* getStatus() - get instance of [Status](#Status) interface.
- IProvider* getDispatcher() - get instance of [Provider](#Provider) interface, implemented by yValve (main provider
  instance).
- IPluginManager* getPluginManager() - get instance of [PluginManager](#PluginManager) interface.
- ITimerControl* getTimerControl() - get instance of [TimerControl](#TimerControl) interface.
- IDtc* getDtc() - get instance of [Dtc](#Dtc) interface.
- IUtil* getUtilInterface() - get instance of [Util](#Util) interface.
- IConfigManager* getConfigManager() - get instance of [ConfigManager](#ConfigManager) interface.

<a name="MessageMetadata"></a> MessageMetadata interface – partial analogue of XSQLDA (does not contain message data,
only message format info is present). Used in a calls related with execution of SQL statements.

- unsigned getCount(StatusType* status) – returns number of fields/parameters in a message. In all calls, containing
  index parameter, it's value should be: 0 <= index < getCount().
- const char* getField(StatusType* status, unsigned index) – returns field name.
- const char* getRelation(StatusType* status, unsigned index) – returns relation name (from which given field is
  selected).
- const char* getOwner(StatusType* status, unsigned index) - returns relation's owner name.
- const char* getAlias(StatusType* status, unsigned index) - returns field alias.
- unsigned getType(StatusType* status, unsigned index) - returns field SQL type.
- FB_BOOLEAN isNullable(StatusType* status, unsigned index) - returns true if field is nullable.
- int getSubType(StatusType* status, unsigned index) - returns blof field subtype (0 – binary, 1 – text, etc.).
- unsigned getLength(StatusType* status, unsigned index) - returns maximum field length.
- int getScale(StatusType* status, unsigned index) - returns scale factor for numeric field.
- unsigned getCharSet(StatusType* status, unsigned index) - returns character set for character field and text blob.
- unsigned getOffset(StatusType* status, unsigned index) - returns offset of field data in message buffer (use it to
  access data in message buffer).
- unsigned getNullOffset(StatusType* status, unsigned index) - returns offset of null indicator for a field in message
  buffer.
- IMetadataBuilder* getBuilder(StatusType* status) - returns [MetadataBuilder](#MessageMetadata) interface initialized
  with this message metadata.
- unsigned getMessageLength(StatusType* status) - returns length of message buffer (use it to allocate memory for the
  buffer).
- unsigned getAlignment(StatusType* status) – returns alignment required for message buffer.
- unsigned getAlignedLength(StatusType* status) – returns length of message buffer taking into an account alignment
  requirements (use it to allocate memory for an array of buffers and navigate through that array).

<a name="MetadataBuilder"></a> MetadataBuilder interface – makes it possible to coerce datatypes in existing message or
construct metadata from the beginning.

- void setType(StatusType* status, unsigned index, unsigned type) – set SQL type of a field.
- void setSubType(StatusType* status, unsigned index, int subType) – set blof field subtype.
- void setLength(StatusType* status, unsigned index, unsigned length) – set maximum length of character field.
- void setCharSet(StatusType* status, unsigned index, unsigned charSet) – set character set for character field and text
  blob.
- void setScale(StatusType* status, unsigned index, unsigned scale) – set scale factor for numeric field
- void truncate(StatusType* status, unsigned count) – truncate message to contain not more than count fields.
- void moveNameToIndex(StatusType* status, const char* name, unsigned index) – reorganize fields in a message – move
  field “name” to given position.
- void remove(StatusType* status, unsigned index) – remove field.
- unsigned addField(StatusType* status) – add field.
- IMessageMetadata* getMetadata(StatusType* status) – get [MessageMetadata](#10. MessageMetadata) interface built by
  this builder.
- void setField(StatusType* status, uint index, const string field) – set name of a field / column.
- void setRelation(StatusType* status, uint index, const string relation) – set name of the relation from which the
  field was selected.
- void setOwner(StatusType* status, uint index, const string owner) – set name of that relation owner.
- void setAlias(StatusType* status, uint index, const string alias) – set alias name of the field in related statement.

<a name="OffsetsCallback"></a> OffsetsCallback interface:

- setOffset(StatusType* status, unsigned index, unsigned offset, unsigned nullOffset) – notifies that offsets for
  field/parameter number “index” should be set to passed values. Should be implemented by user when implementing
  [MessageMetadata](#MessageMetadata) interface and using [Util](#Util)::setOffsets().

<a name="PluginConfig"></a> PluginConfig interface – passed to plugin's factory when plugin instance (with particular
configuration) to be created.

- const char* getConfigFileName() - recommended file name where configuration for plugin is expected to be stored.
- IConfig* getDefaultConfig(StatusType* status) – plugin configuration loaded with default rules.
- IFirebirdConf* getFirebirdConf(StatusType* status) – master firebird configuration taking into an account per-database
  settings for a database with which will work new instance of plugin.
- void setReleaseDelay(StatusType* status, ISC_UINT64 microSeconds) – used by plugin to configure recommended delay
  during which plugin module will not be unloaded by plugin manager after release of last plugin instance from that
  module.

<a name="PluginFactory"></a> PluginFactory interface – should be implemented by plugin author when writing plugin.

- IPluginBase* createPlugin(StatusType* status, IPluginConfig* factoryParameter) – creates new instance of plugin with
  passed recommended configuration.

<a name="PluginManager"></a> PluginManager interface – API of plugin manager.

- void registerPluginFactory(unsigned pluginType, const char* defaultName, IPluginFactory* factory) – registers named
  factory of plugins of given type.
- void registerModule(IPluginModule* cleanup) – registers plugin module.
- void unregisterModule(IPluginModule* cleanup) – unregisters plugin module (in case of unexpected unload by OS).
- IPluginSet* getPlugins(StatusType* status, unsigned pluginType, const char* namesList, IFirebirdConf* firebirdConf) –
  returns PluginSet interface providing access to list of plugins of given type. Names of plugins to be included are
  taken from namesList, when missing (NULL) – from configuration setting for given pluginType. If firebirdConf parameter
  is specified it is used for all configuration purporses (including getting list of plugins and passing to
  [PluginFactory](#PluginFactory)::createPlugin() method), if missing (NULL) – default configuration (from
  firebird.conf) is used.
- IConfig* getConfig(StatusType* status, const char* filename) – returns Config interface for given configuration file
  name. Can be used by plugins to access configuration files with standard format but non-default name.
- void releasePlugin(IPluginBase* plugin) – release given plugin. Should be used for plugins instead simple release()
  due to need to perform additional actions with plugin owner before actual release.

Constants defined by PluginManager interface (plugin types):

- TYPE_PROVIDER
- TYPE_AUTH_SERVER
- TYPE_AUTH_CLIENT
- TYPE_AUTH_USER_MANAGEMENT
- TYPE_EXTERNAL_ENGINE
- TYPE_TRACE
- TYPE_WIRE_CRYPT
- TYPE_DB_CRYPT
- TYPE_KEY_HOLDER

<a name="PluginModule"></a> PluginModule interface – represents plugin module (dynamic library). Should be implemented
by plugin author in each plugin module (one instance per module).

- void doClean() - called by plugin manager before normal unload of plugin module.

PluginSet interface – represents set of plugins of given type. Typically used by internal firebird code but recommended
for use in plugins loading other plugins.

- const char* getName() - get name of current plugin in a set.
- const char* getModuleName() - get name of a module of current plugin in a set (in simple case matches plugin name).
- IPluginBase* getPlugin(StatusType* status) – get an instance of current plugin, returned interface should be casted to
  main interface of plugin of requested in [PluginManager](#PluginManager)::getPlugins() type. Returns NULL if set does
  not contain any more plugins. Reference counter of plugin, returned by this function, is incremented on return – do
  not forget to use releasePlugin() method of [PluginManager](#PluginManager) for plugins returned by this method.
- void next(StatusType* status) – make set to switch to next plugin from the list.
- void set(StatusType* status, const char* list) – reset interface: make it work with list of plugins provided by list
  parameter. Type of plugins remains unchanged.

<a name="Provider"></a> Provider interface – main interface to start database / service access.

- IAttachment* attachDatabase(StatusType* status, const char* fileName, unsigned dpbLength, const unsigned char* dpb) –
  replaces isc_attach_database().
- IAttachment* createDatabase(StatusType* status, const char* fileName, unsigned dpbLength, const unsigned char* dpb) –
  replaces isc_create_database().
- IService* attachServiceManager(StatusType* status, const char* service, unsigned spbLength, const unsigned char* spb)
  – replaces isc_service_attach().
- void shutdown(StatusType* status, unsigned timeout, const int reason) – replaces fb_shutdown().
- void setDbCryptCallback(StatusType* status, ICryptKeyCallback* cryptCallback) – sets database encryption callback
  interface that will be used for following database and service attachments. See … for details.

<a name="ResultSet"></a> ResultSet interface – replaces (with extended functionality) some functions of isc_stmt_handle.
This interface is returned by openCursor() call in [IAttachment](#Attachment) or [IStatement](#Statement). All fetch
calls except fetchNext() work only for bidirectional (opened with CURSOR_TYPE_SCROLLABLE flag) result set.

- int fetchNext(StatusType* status, void* message) – fetch next record, replaces isc_dsql_fetch(). This method (and
  other fetch methods) returns [completion code](#Completion codes) Status::RESULT_NO_DATA when EOF is reached,
  Status::RESULT_OK on success.
- int fetchPrior(StatusType* status, void* message) – fetch previous record.
- int fetchFirst(StatusType* status, void* message) – fetch first record.
- int fetchLast(StatusType* status, void* message) – fetch last record.
- int fetchAbsolute(StatusType* status, int position, void* message) – fetch record by it's absolute position in result
  set.
- int fetchRelative(StatusType* status, int offset, void* message) – fetch record by position relative to current.
- FB_BOOLEAN isEof(StatusType* status) – check for EOF.
- FB_BOOLEAN isBof(StatusType* status) – check for BOF.
- IMessageMetadata* getMetadata(StatusType* status) – get metadata for messages in result set, specially useful when
  result set is opened by [IAttachment](#Attachment)::openCursor() call with NULL output metadata format parameter (this
  is the only way to obtain message format in this case).
- void close(StatusType* status) – close result set, releases interface on success.
- void getInfo(StatusType* status, unsigned itemsLength, const unsigned char* items, unsigned bufferLength, unsigned
  char* buffer) – retrieve information about result set. Items accepted in getInfo() call:
  - INF_RECORD_COUNT – number of records stored inside a scrollable cursor, or -1 for a uni-directional cursor.

<a name="Service"></a> Service interface – replaces isc_svc_handle.

- void detach(StatusType* status) – close attachment to services manager, on success releases interface. Replaces
  isc_service_detach().
- void query(StatusType* status, unsigned sendLength, const unsigned char* sendItems, unsigned receiveLength, const
  unsigned char* receiveItems, unsigned bufferLength, unsigned char* buffer) – send and request information to/from
  service, with different receiveItems may be used for both running services and to obtain various server-wide
  information. Replaces isc_service_query().
- void start(StatusType* status, unsigned spbLength, const unsigned char* spb) – start utility in services manager.
  Replaces isc_service_start().
- void cancel(StatusType* status) – cancel wait of current query() call. Supported only for embedded connections.

<a name="Statement"></a> Statement interface – replaces (partially) isc_stmt_handle.

- void getInfo(StatusType* status, unsigned itemsLength, const unsigned char* items, unsigned bufferLength, unsigned
  char* buffer) – replaces isc_dsql_sql_info().
- unsigned getType(StatusType* status) – statement type, currently can be found only in firebird sources in dsql/dsql.h.
- const char* getPlan(StatusType* status, FB_BOOLEAN detailed) – returns statement execution plan.
- ISC_UINT64 getAffectedRecords(StatusType* status) – returns number of records affected by statement.
- IMessageMetadata* getInputMetadata(StatusType* status) – returns parameters metadata.
- IMessageMetadata* getOutputMetadata(StatusType* status) – returns output values metadata.
- ITransaction* execute(StatusType* status, ITransaction* transaction, IMessageMetadata* inMetadata, void* inBuffer,
  IMessageMetadata* outMetadata, void* outBuffer) – executes any SQL statement except returning multiple rows of data.
  Partial analogue of isc_dsql_execute2() - in and out XSLQDAs replaced with input and output messages with appropriate
  buffers.
- IResultSet* openCursor(StatusType* status, ITransaction* transaction, IMessageMetadata* inMetadata, void* inBuffer,
  IMessageMetadata* outMetadata, unsigned flags) – executes SQL statement potentially returning multiple rows of data.
  Returns [ResultSet](#ResultSet) interface which should be used to fetch that data. Format of output data is defined by
  outMetadata parameter, leaving it NULL default format may be used. Parameter flags is needed to open bidirectional
  cursor setting it's value to Istatement::CURSOR_TYPE_SCROLLABLE.
- IBatch* createBatch(StatusType* status, IMessageMetadata* inMetadata, uint parLength, const uchar* par) – creates
  [Batch](#Batch) interface to SQL statement with input parameters making it possible to execute that statement with
  multiple sets of parameters. Format of input data is defined by inMetadata parameter, leaving it NULL makes batch use
  default format from this interface. Parameters block may be passed to createBatch() making it possible to adjust batch
  behavior.
- void setCursorName(StatusType* status, const char* name) – replaces isc_dsql_set_cursor_name().
- void free(StatusType* status) – free statement, releases interface on success.
- unsigned getFlags(StatusType* status) – returns [flags](#Values returned by getFlags) describing how this statement
  should be executed, simplified replacement of getType() method.

Constants defined by Statement interface:

IAttachment::prepare() flags:

- PREPARE_PREFETCH_NONE – constant to pass no flags, 0 value.

The following flags may be OR-ed to get desired set of flags:

- PREPARE_PREFETCH_TYPE
- PREPARE_PREFETCH_INPUT_PARAMETERS
- PREPARE_PREFETCH_OUTPUT_PARAMETERS
- PREPARE_PREFETCH_LEGACY_PLAN
- PREPARE_PREFETCH_DETAILED_PLAN
- PREPARE_PREFETCH_AFFECTED_RECORDS
- PREPARE_PREFETCH_FLAGS (flags returned by getFlags() method)

Frequently used combinations of flags:

- PREPARE_PREFETCH_METADATA
- PREPARE_PREFETCH_ALL

<a name="Values returned by getFlags"></a> Values returned by getFlags() method:

- FLAG_HAS_CURSOR – use openCursor() to execute this statement, not execute()
- FLAG_REPEAT_EXECUTE – when prepared statement may be executed many times with different parameters

<a name="Flags passed to openCursor"></a> Flags passed to openCursor():

- CURSOR_TYPE_SCROLLABLE – open bidirectional cursor.

<a name="Status"></a> Status interface – replaces ISC_STATUS_ARRAY. Functionality is extended – Status has separate
access to errors and warnings vectors, can hold vectors of unlimited length, itself stores strings used in vectors
avoiding need in circular strings buffer. In C++ Status is always used under status wrapper, C++ API provides two
different [wrappers](#Status Wrapper) having different behavior when error is returned from API call. Interface is on
purpose minimized (methods like convert it to text are moved to [Util](#Util) interface) in order to simplify it's
implementation by users when needed.

- void init() - cleanup interface, set it to initial state.
- unsigned getState() - get current state of interface, returns [state flags](#returned by getState), may be OR-ed.
- void setErrors2(unsigned length, const intptr_t* value) – set contents of errors vector with length explicitly
  specified in a call.
- void setWarnings2(unsigned length, const intptr_t* value) – set contents of warnings vector with length explicitly
  specified in a call.
- void setErrors(const intptr_t* value) – set contents of errors vector, length is defined by value context.
- void setWarnings(const intptr_t* value) – set contents of warnings vector, length is defined by value context.
- const intptr_t* getErrors() - get errors vector.
- const intptr_t* getWarnings() - get warnings vector.
- IStatus* clone() - create clone of current interface.

Constants defined by Status interface:

<a name="returned by getState"></a> Flags set in the value, returned by getState() method:

- STATE_WARNINGS
- STATE_ERRORS

<a name="Completion codes"></a> Completion codes:

- RESULT_ERROR
- RESULT_OK
- RESULT_NO_DATA
- RESULT_SEGMENT

<a name="Timer"></a> Timer interface – user timer. Callback interface which should be implemented by user to use
firebird timer.

- void handler() - method is called when timer rings (or when server is shutting down).

<a name="TimerControl"></a> TimerControl interface – very simple and not too precise implementation of timer. Arrived
here because existing timers are very much OS dependent and may be used in programs that require to be portable and do
not need really high precision timer. Particularly execution of given timer may be delayed if another one has not
completed at the moment when given timer should alarm.

- void start(StatusType* status, ITimer* timer, ISC_UINT64 microSeconds) – start [ITimer](#Timer) to alarm after given
  delay (in microseconds, 10 <sup>-6</sup> seconds). Timer will be waked up only once after this call.
- void stop(StatusType* status, ITimer* timer) – stop [ITimer](#Timer). It's not an error to stop not started timer thus
  avoiding problems with races between stop() and timer alarm.

<a name="Transaction"></a> Transaction interface – replaces isc_tr_handle.

- void getInfo(StatusType* status, unsigned itemsLength, const unsigned char* items, unsigned bufferLength, unsigned
  char* buffer) – replaces isc_transaction_info().
- void prepare(StatusType* status, unsigned msgLength, const unsigned char* message) – replaces
  isc_prepare_transaction2(), with zero msgLength behaves like isc_prepare_transaction() automatically generating
  appropriate message.
- void commit(StatusType* status) – replaces isc_commit_transaction().
- void commitRetaining(StatusType* status) – replaces isc_commit_retaining().
- void rollback(StatusType* status) – replaces isc_rollback_transaction().
- void rollbackRetaining(StatusType* status) – replaces isc_rollback_retaining().
- void disconnect(StatusType* status) – replaces fb_disconnect_transaction().
- ITransaction* join(StatusType* status, ITransaction* transaction) – joins current transaction and passed as parameter
  transaction into single distributed transaction (using [Dtc](#Dtc)). On success both current transaction and passed as
  parameter transaction are released and should not be used any more.
- ITransaction* validate(StatusType* status, IAttachment* attachment) – this method is used to support distributed
  transactions coordinator.
- ITransaction* enterDtc(StatusType* status) – this method is used to support distributed transactions coordinator.

<a name="VersionCallback"></a> VersionCallback interface – callback for [Util](#Util)::getFbVersion().

- void callback(StatusType* status, const char* text) – called by firebird engine for each line in multiline version
  report. Makes it possible to print that lines one by one, place them into message box in any GUI, etc.

<a name="Util"></a> Util interface – various helper methods required here or there.

- void getFbVersion(StatusType* status, IAttachment* att, IVersionCallback* callback) – produce long and beautiful
  report about firebird version used. It may be seen in ISQL when invoked with -Z switch.
- void loadBlob(StatusType* status, ISC_QUAD* blobId, IAttachment* att, ITransaction* tra, const char* file, FB_BOOLEAN
  txt) – load blob from file.
- void dumpBlob(StatusType* status, ISC_QUAD* blobId, IAttachment* att, ITransaction* tra, const char* file, FB_BOOLEAN
  txt) – save blob to file.
- void getPerfCounters(StatusType* status, IAttachment* att, const char* countersSet, ISC_INT64* counters) – get
  statistics for given attachment.
- IAttachment* executeCreateDatabase(StatusType* status, unsigned stmtLength, const char* creatDBstatement, unsigned
  dialect, FB_BOOLEAN* stmtIsCreateDb) – execute “CREATE DATABASE ...” statement – ISC trick with NULL statement handle
  does not work with interfaces.
- void decodeDate(ISC_DATE date, unsigned* year, unsigned* month, unsigned* day) – replaces isc_decode_sql_date().
- void decodeTime(ISC_TIME time, unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions) – replaces
  isc_decode_sql_time().
- ISC_DATE encodeDate(unsigned year, unsigned month, unsigned day) – replaces isc_encode_sql_date().
- ISC_TIME encodeTime(unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions) – replaces
  isc_encode_sql_time().
- unsigned formatStatus(char* buffer, unsigned bufferSize, IStatus* status) – replaces fb_interpret(). Size of buffer,
  passed into this method, should not be less than 50 bytes.
- unsigned getClientVersion() – returns integer, containing major version in byte 0 and minor version in byte 1.
- IXpbBuilder* getXpbBuilder(StatusType* status, unsigned kind, const unsigned char* buf, unsigned len) – returns
  [XpbBuilder](#XpbBuilder) interface. Valid [kinds](#Valid builder types) are enumerated in [XpbBuilder](#XpbBuilder).
- unsigned setOffsets(StatusType* status, IMessageMetadata* metadata, IOffsetsCallback* callback) – sets valid offsets
  in [MessageMetadata](#MessageMetadata). Performs calls to callback in [OffsetsCallback](#OffsetsCallback) for each
  field/parameter.
- IDecFloat16* getDecFloat16(StatusType* status) – access [DecFloat16](#DecFloat16) interface.
- IDecFloat34* getDecFloat34(StatusType* status) – access [DecFloat34](#DecFloat34) interface.
- void decodeTimeTz(StatusType* status, const ISC_TIME_TZ* timeTz, unsigned* hours, unsigned* minutes, unsigned*
  seconds, unsigned* fractions, unsigned timeZoneBufferLength, char* timeZoneBuffer) – decode time taking time zone into
  an account.
- void decodeTimeStampTz(StatusType* status, const ISC_TIMESTAMP_TZ* timeStampTz, unsigned* year, unsigned* month,
  unsigned* day, unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions, unsigned
  timeZoneBufferLength, char* timeZoneBuffer) - decode timestamp taking time zone into an account.
- void encodeTimeTz(StatusType* status, ISC_TIME_TZ* timeTz, unsigned hours, unsigned minutes, unsigned seconds,
  unsigned fractions, const char* timeZone) – encode time taking time zone into an account.
- void encodeTimeStampTz(StatusType* status, ISC_TIMESTAMP_TZ* timeStampTz, unsigned year, unsigned month, unsigned day,
  unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions, const char* timeZone) – encode timestamp
  taking time zone into an account.
- IInt128* getInt128(Status status) – access [Int128](#Int128) interface.
- void decodeTimeTzEx(Status status, const ISC_TIME_TZ_EX* timeTz, uint* hours, uint* minutes, uint* seconds, uint*
  fractions, uint timeZoneBufferLength, string timeZoneBuffer) – decode time taking extended time zone into an account.
- void decodeTimeStampTzEx(Status status, const ISC_TIMESTAMP_TZ_EX* timeStampTz, uint* year, uint* month, uint* day,
  uint* hours, uint* minutes, uint* seconds, uint* fractions, uint timeZoneBufferLength, string timeZoneBuffer) – decode
  timestamp taking extended time zone into an account.
- Attachment executeCreateDatabase2(Status status, uint stmtLength, const string creatDBstatement, uint dialect,
  uint dpbLength, const uchar* dpb, boolean* stmtIsCreateDb);
- void convert(Status status, uint sourceType, uint sourceScale, uint sourceLength, const void* source,
  uint targetType, uint targetScale, uint targetLength, void* target);

<a name="XpbBuilder"></a> XpbBuilder methods:

- void clear(StatusType* status) – reset builder to empty state.
- void removeCurrent(StatusType* status) – removes current clumplet.
- void insertInt(StatusType* status, unsigned char tag, int value) – inserts a clumplet with value representing integer
  in network format.
- void insertBigInt(StatusType* status, unsigned char tag, ISC_INT64 value) – inserts a clumplet with value representing
  integer in network format.
- void insertBytes(StatusType* status, unsigned char tag, const void* bytes, unsigned length) - inserts a clumplet with
  value containing passed bytes.
- void insertTag(StatusType* status, unsigned char tag) – inserts a clumplet without a value.
- FB_BOOLEAN isEof(StatusType* status) – checks that there is no current clumplet.
- void moveNext(StatusType* status) – moves to next clumplet.
- void rewind(StatusType* status) – moves to first clumplet.
- FB_BOOLEAN findFirst(StatusType* status, unsigned char tag) – finds first clumplet with given tag.
- FB_BOOLEAN findNext(StatusType* status) – finds next clumplet with given tag.
- unsigned char getTag(StatusType* status) – returns tag for current clumplet.
- unsigned getLength(StatusType* status) – returns length of current clumplet value.
- int getInt(StatusType* status) – returns value of current clumplet as integer.
- ISC_INT64 getBigInt(StatusType* status) – returns value of current clumplet as 64-bit integer.
- const char* getString(StatusType* status) – returns value of current clumplet as pointer to zero-terminated string
  (pointer is valid till next call to this method).
- const unsigned char* getBytes(StatusType* status) – returns value of current clumplet as pointer to unsigned char.
- unsigned getBufferLength(StatusType* status) – returns length of parameters block.
- const unsigned char* getBuffer(StatusType* status) – returns pointer to parameters block.

Constants defined by XpbBuilder interface:

<a name="Valid builder types"></a> Valid builder types:

- BATCH (IBatch parameters block)
- BPB (BLOB parameters block)
- DPB (database attachment parameters block)
- SPB_ATTACH (service attachment parameters block)
- SPB_START (start service parameters)
- SPB_SEND (send items in [IService](#Service)::query())
- SPB_RECEIVE (receive items in [IService](#Service)::query())
- SPB_RESPONSE (response from [IService](#Service)::query())
- TPB (transaction parameters block)

# Plugin, encrypting data transferred over the wire

Algorithms performing encryption of data for different purposes are well known for many years. The only “little” typical
problem remaining is where to get the top secret key to be used by that algorithm. Luckily for network traffic
encryption there is one good solution – unique encryption key should be generated by authentication plugin. At least
default SRP plugin can produce such a key. And that key is resistant to attacks, including man-in-the-middle. Therefore
was chosen a method of providing keys for wire crypt plugin: get it from authentication plugin. (In case when used
authentication plugin can not provide a key a pseudo-plugin may be added in AuthClient and AuthServer lists to produce
keys, something like two asymmetric private/public pairs.)

CryptKey interface is used to store a key provided by authentication plugin and pass it to wire crypt plugin. This
interface should be used as follows – when server or client authentication plugin is ready to provide a key it asks
[ServerBlock](#ServerBlock) or [ClientBlock](#ClientBlock) to produce new CryptKey interface and stores a key in it.
Appropriate for [WireCryptPlugin](#WireCryptPlugin) type of key will be selected by firebird and passed to that
interface.

- void setSymmetric(StatusType* status, const char* type, unsigned keyLength, const void* key) – make it store symmetric
  key of given type.
- void setAsymmetric(StatusType* status, const char* type, unsigned encryptKeyLength, const void* encryptKey, unsigned
  decryptKeyLength, const void* decryptKey) – make it store pair of asymmetric keys of given type.
- const void* getEncryptKey(unsigned* length) – get a key for encryption.
- const void* getDecryptKey(unsigned* length) – get a key for decryption (in case of symmetric key produces same result
  as getEncryptKey()).

<a name="WireCryptPlugin"></a> WireCryptPlugin interface is main interface of network crypt plugin. Like any other such
interface it should be implemented by author of the plugin.

- const char* getKnownTypes(StatusType* status) – returns whitespace/tab/comma separated list of acceptable keys.
- void setKey(StatusType* status, ICryptKey* key) – plugin should use a key passed to it by this call.
- void encrypt(StatusType* status, unsigned length, const void* from, void* to) – encrypts a packet to be sent over the
  wire.
- void decrypt(StatusType* status, unsigned length, const void* from, void* to) – decrypts a packet received from
  network.

# Server side of authentication plugin

Authentication plugin contains two required parts – client and server and may also contain related third part - user
manager. During authentication process firebird client invokes client plugin and sends generated by it data to server,
next server invokes server plugin and sends generated by it data to client. This process repeats as long as both plugins
return AUTH_MORE_DATA code. AUTH_SUCCESS returned at server side means successful authentication, AUTH_FAILED at any
side – immediate abort of iterative process and failure reported to client, AUTH_CONTINUE means that next plugin from
the list of configured authentication plugins should be tried.

There is no dedicated sample of authentication plugins but in firebird sources in directory src/auth one can find
AuthDbg plugin using which one can learn on trivial example (no complex calculations like in Srp and no calls to crazy
WinAPI functions like in AuthSspi) how client and server sides perform authentication handshake.

Auth interface does not contain methods, only some constants defining codes return from authenticate() method of
[Client](#Client) and [Server](#Server).

- AUTH_FAILED
- AUTH_SUCCESS
- AUTH_MORE_DATA
- AUTH_CONTINUE

<a name="Writer"></a> Writer interface – writes authentication parameters block.

- void reset() - clear target block.
- void add(StatusType* status, const char* name) – add login name.
- void setType(StatusType* status, const char* value) – set type of added login (user, group, etc).
- void setDb(StatusType* status, const char* value) – set security database in which authentication was done.

<a name="ServerBlock"></a> ServerBlock interface is used by server side of authentication plugin to exchange data with
client.

- const char* getLogin() - get login name passed from client.
- const unsigned char* getData(unsigned* length) – get authentication data passed from client.
- void putData(StatusType* status, unsigned length, const void* data) – pass authentication data to client.
- ICryptKey* newKey(StatusType* status) – create new wire crypt key and add it to the list of available for wire crypt
  plugins.

<a name="Server"></a> Server interface is main interface of server side of authentication plugin.

- int authenticate(StatusType* status, IServerBlock* sBlock, IWriter* writerInterface) – perform single authentication
  step. Data exchange with client is performed using sBlock interface. When some authentication item is produced it
  should be added to authentication block using writerInterface. Possible return values are defined in [Auth](#Auth)
  interface.

# Client side of authentication plugin

<a name="ClientBlock"></a> ClientBlock interface is used by client side of authentication plugin to exchange data with
server.

- const char* getLogin() - get login name if it is present in DPB.
- const char* getPassword() - get password if it is present in DPB.
- const unsigned char* getData(unsigned* length) – get authentication data passed from server.
- void putData(StatusType* status, unsigned length, const void* data) – pass authentication data to server.
- ICryptKey* newKey(StatusType* status) - create new wire crypt key and add it to the list of available for wire crypt
  plugins.

<a name="Client"></a> Client interface is main interface of client side of authentication plugin.

- int authenticate(StatusType* status, IClientBlock* cBlock) – perform single authentication step. Data exchange with
  server is performed using cBlock interface. Possible return values are defined in Auth interface. AUTH_SUCCESS is
  treated by client side as AUTH_MORE_DATA (i.e. client sends generated data to server and waits for an answer from it).

# User management plugin

This plugin is actively related with server side of authentication – it prepares users' list for authentication plugin.
Not each authentication plugin requires user manager – some may access list of users created using non-firebird software
(AuthSspi for example). Record describing user consists of a number of fields and operation which should be performed
like add user, modify user, list user(s), etc. Plugin must interpret commands received in [User](#User) interface.

<a name="UserField"></a> UserField interface is not used as standalone interface, it's base for CharUserField and
IntUserField.

- int entered() - returns non-zero if a value for a field was entered (assigned).
- int specified() - return non-zero if NULL value was assigned to the field.
- void setEntered(StatusType* status, int newValue) – sets entered flag to 0/non-zero value for a field. There is no
  method to assign NULL for a field cause it's never needed. NULLs if used are assigned by the code implementing
  interfaces and therefore having full access to internals of them.

<a name="CharUserField"></a> CharUserField interface:

- const char* get() - get field's value as C-string (\0 terminated).
- void set(StatusType* status, const char* newValue) – assigns value to the field. Sets entered flag to true.

<a name="IntUserField"></a> IntUserField interface:

- int get() - get field's value.
- void set(StatusType* status, int newValue) – assigns value to the field. Sets entered flag to true.

<a name="User"></a> User interface is a list of methods accessing fields included into record about the user.

- unsigned operation() - code of operation (see [list](#Constants defined by User interface) below).
- ICharUserField* userName() - login name.
- ICharUserField* password() - password. Always empty when listing users.
- ICharUserField* firstName() - this and 2 next are components of full user name.
- ICharUserField* lastName()
- ICharUserField* middleName()
- ICharUserField* comment() - comment (from SQL operator COMMENT ON USER IS …).
- ICharUserField* attributes() - tags in a form tag1=val1, tag2=val2, …, tagN=valN. Val may be empty – than means that
  tag will be deleted.
- IIntUserField* active() - changes ACTIVE/INACTIVE setting for user.
- IIntUserField* admin() - sets/drops admin rights for the user.
- void clear(StatusType* status) – sets all fields to not entered and not specified.

<a name="Constants defined by User interface"></a> Constants defined by User interface – valid codes of operation.

- OP_USER_ADD – create user
- OP_USER_MODIFY – alter user
- OP_USER_DELETE – drop user
- OP_USER_DISPLAY – show user
- OP_USER_SET_MAP – turn on mapping of windows admins to role rdb$admin
- OP_USER_DROP_MAP – turn off mapping of windows admins to role rdb$admin

<a name="ListUsers"></a> ListUsers interface is callback used by authentication plugin when list users operation is
requested. Plugin fills [User](#User) interface for all items in list of users one by one and for each user calls list()
method of this interface.

- void list(StatusType* status, IUser* user) – callback function. Implementation can do what it wants with received
  data. For example, it may put data from user parameter into output stream of listing service or place into special
  tables from SEC$ group.

<a name="LogonInfo"></a> LogonInfo interface contains data passed to user mamngement plugin to attach to security
database with valid credentials. Pres

- const char* name() - returns current attachment's login name.
- const char* role() - returns current attachment's active role.
- const char* networkProtocol() - returns current attachment's network protocol. Currently not used by plugins.
- const char* remoteAddress() - returns current attachment's remote address. Currently not used by plugins.
- const unsigned char* authBlock(unsigned* length) – returns current attachment's authentication block. When not NULL
  overrides login name.

<a name="Management"></a> Management interface is main interface of user management plugin.

- void start(StatusType* status, ILogonInfo* logonInfo) – starts plugin, if needed it attaches to security database to
  manage may be (it's plugin-dependent solution use it or not) using credentials from logonInfo.
- int execute(StatusType* status, IUser* user, IListUsers* callback) – executes a command provided by operation() method
  of user parameter. If needed callback interface will be used. Parameter callback may have NULL value for commands not
  requiring users' listing.
- void commit(StatusType* status) – commits changes done by calls to execute() method.
- void rollback(StatusType* status) – rollbacks changes done by calls to execute() method.

# Database encryption plugin

An ability to encrypt database was present in firebird since interbase times but appropriate places in the code were
commented. Implementation was suspicious – crypt key was always sent from the client in DPB, no attempt was made to hide
it from the world and no way was suggested to encrypt existing database. FB3 solves most of the problems except probably
the worst one – how to manage crypt keys. We suggest a kind of solution but it requires efforts in plugins, i.e. there
is no beautiful way to work with keys like it is for wire crypt plugins.

Before starting with own db crypt plugin one should take into an account the following. We see two main usages of
database encryption – first, it may be needed to avoid data loss if database server is physically stolen, and second, it
may be used to protect data in database which is sailed together with special application accessing that data.
Requirements for this usages are quite different. In first case we may trust database server that it is not modified to
steal keys passed to security plugin – i.e. we expect that key will not be sent to inappropriate server. In second case
server may be modified in some way to steal keys (if they are passed from application to plugin via server code) or even
data (as the last resort to dump blocks from the cache where they are in non-encrypted form). Therefore your plugin
should make sure that it's running with not modified firebird binaries and your application before sending a key to
plugin should make sure it's really required plugin, may be asking a kind of digital signature from it. Making sure that
network line is encrypted (parsing output of [Util](#Util)::getFbVersion()) or using some own encryption of a key is
also very good idea in case when network access to the server is used. All this job should be done in plugin (and
application working with it) i.e. database block encryption algorithm by itself may happen to be easiest part of db
crypt plugin, specially when some standard library is used for it.

<a name="CryptKeyCallback"></a> CryptKeyCallback interface should be provided by a side sending crypt key to db crypt
plugin or key holder plugin.

- unsigned callback(unsigned dataLength, const void* data, unsigned bufferLength, void* buffer) – when performing
  callback information is passed in both directions. The source of a key receives dataLength bytes of data and may send
  up to bufferLength bytes into buffer returning actual number of bytes placed into buffer.
- void dispose() - invoked when interface is not needed any more. Helps to avoid memory leaks in statefull inrefaces.
- unsigned afterAttach(StatusType* status, const char* dbName, const IStatus* attStatus) - invoked after attach on
  client system. NULL in attStatus means attach was successful but afterAttach() is anyway invoked in order to let
  plugin perform required cleanup. The following values may be returned by this function: NO_RETRY - do not repeat
  attempts to attach to database.
    DO_RETRY - retry attach (ignored when function was called without attStatus).
- int getHashLength(StatusType* status) - returns length of hash of the keys provided by interface. Hash is needed to
  let caller compare interfaces for equality. Empty (no keys) interface should return hash length equal to zero.
- void getHashData(StatusType* status, void* hash) - places hash of the keys provided by interface into memory location
  defined by parameter _hash_. Should copy exact number of bytes returned by getHashLength() call. Hash algorithm is
  chosen by plugin author (in year 2024 SHA256 appears reasonable choice).

<a name="DbCryptInfo"></a> DbCryptInfo interface is passed to DbCryptPlugin by engine. Plugin may save this interface
and use when needed to obtain additional information about database.

- const char* getDatabaseFullPath(StatusType* status) – returns full (including path) name of primary database file.

<a name="DbCryptPlugin"></a> DbCryptPlugin interface is main interface of database crypt plugin.

- void setKey(StatusType* status, unsigned length, IKeyHolderPlugin** sources, const char* keyName) – is used to provide
  to db crypt plugin information about encryption key. Firebird never passes keys for this type of plugin directly.
  Instead array of [KeyHolderPlugins](#KeyHolderPlugin) of given length is passed to crypt plugin which must get from
  one of it [CryptKeyCallback](#CryptKeyCallback) interface and get a key using it. Parameter keyName is a name of a key
  like it was entered in “ALTER DATABASE ENCRYPT …” operator.
- void encrypt(StatusType* status, unsigned length, const void* from, void* to) – encrypts data before writing block to
  database file.
- void decrypt(StatusType* status, unsigned length, const void* from, void* to) – decrypts data after reading block from
  database file.
- void setInfo(StatusType* status, IDbCryptInfo* info) – in this method crypt plugin typically saves informational
  interface for future use.

# Key holder for database encryption plugin

This type of plugin is needed to delineate functionality – db crypt plugin is dealing with actual encryption, key holder
solves questions related with providing it a key in secure way. Plugin may be received from application or loaded in
some other way (up to using flash device inserted into server at firebird startup time).

<a name="KeyHolderPlugin"></a> KeyHolderPlugin interface is main interface of database crypt key holder plugin.

- int keyCallback(StatusType* status, ICryptKeyCallback* callback) – is used to pass attachment's
  [CryptKeyCallback](#CryptKeyCallback) interface (if provided by user with [Provider](#Provider)::setDbCryptCallback()
  call). This call is always performed at database attach moment, and some holders may reject attachment if satisfactory
  key was not provided. Return value of 0 means that key holder can not provide a key to crypt plugin, non-zero – can.
- ICryptKeyCallback* keyHandle(StatusType* status, const char* keyName) – is intended to be called by
  [DbCryptPlugin](#DbCryptPlugin) directly to obtain callback interface for named key from key holder. This makes it
  possible for open source firebird code to never touch actual keys avoiding ability to steal a key changing firebird
  code. After getting [CryptKeyCallback](#CryptKeyCallback) interface crypt plugin starts data exchange using it. Key
  holder can (for example) check digital signature of crypt plugin before sending a key to it in order to avoid use of
  modified crypt plugin able to steal secret key.
- FB_BOOLEAN useOnlyOwnKeys(StatusType* status) – informs firebird engine whether a key, provided by key holder, can be
  used in other attachments. Makes sense only for SuperServer – only it can share database crypt keys between
  attachments. Returning FB_TRUE from this method enforces firebird to make sure that this particular key holder (and
  therefore in turn attachment related to it) provides correct crypt key for any other attachment to this database.
- ICryptKeyCallback* chainHandle(StatusType* status) – support of a chain of key holders. In some cases key has to pass
  through more than single key holder before it reaches db crypt plugin. This is needed (for example) to support execute
  statement in encrypted database. This is just a sample – chains are also used in some other cases. Callback interface,
  returned by this method, may differ from one returned by keyHandle() function (see above). Typically is should be able
  to duplicate one-to-one keys, received by KeyHolderPlugin when keyCallback() function is invoked.

# Non-interface objects used by API (C++ specific header Message.h)

Following 3 classes are used to represent date, time and timestamp (datetime) when using FB_MESSAGE macro. Members of
data structure, representing static message, correspondint to fields of types FB_DATE / FB_TIME / FB_TIMESTAMP will have
a type of one of this classes. Knowing methods / members (which are enough self-descriptive to avoid describing them
here) of this classes is needed to access date and time fields in static messages.

<a name="FbDate"></a> class FbDate methods:

- void decode(IUtil* util, unsigned* year, unsigned* month, unsigned* day)
- unsigned getYear(IUtil* util)
- unsigned getMonth(IUtil* util)
- unsigned getDay(IUtil* util)
- void encode(IUtil* util, unsigned year, unsigned month, unsigned day)

<a name="FbTime"></a> class FbTime methods:

- void decode(IUtil* util, unsigned* hours, unsigned* minutes, unsigned* seconds, unsigned* fractions)
- unsigned getHours(IUtil* util)
- unsigned getMinutes(IUtil* util)
- unsigned getSeconds(IUtil* util)
- unsigned getFractions(IUtil* util)
- void encode(IUtil* util, unsigned hours, unsigned minutes, unsigned seconds, unsigned fractions)

class FbTimestamp members:
- FbDate date;
- FbTime time;

Following two templates are used in static messages to represent CHAR(N) and VARCHAR(N) fields.

```cpp
template <unsigned N>
struct FbChar
{
   char str[N];
};

template <unsigned N>
struct FbVarChar
{
    ISC_USHORT length;
    char str[N];

    void set(const char* s);
};
```

---

This document is currently missing 2 types of plugins – ExternalEngine and Trace. Information about them will be made
available in next release of it.

# Trace plugin

_TODO_

# Trace objects

_TODO_

# Trace performance statistics

Trace plugin may retrieve various performance statistics available using the `getPerfStats()` method of the trace object, which returns a pointer to the `IPerformanceStats` interface.

```cpp
IPerformanceStats* stats = statement->getPerfStats();
```

The returned pointer may be `nullptr` if the corresponding trace object is not in the terminate state yet (i.e. still active / being executed).

<a name="PerformanceStats"></a> PerformanceStats interface:

- ISC_UINT64 getElapsedTime() - returns the elapsed time, in milliseconds
- ISC_UINT64 getFetchedRecords() - returns number of records fetched during execution
- IPerformanceCounters* getCounters(unsigned group) - returns the requested performance counters group

The following groups of performance counters are currently supported:

- COUNTER_GROUP_PAGES - per-pagespace counters
- COUNTER_GROUP_TABLES - per-table counters

If `getCounters()` is called with a counter group not supported by the implementation, `nullptr` is returned.

<a name="PerformanceCounters"></a> PerformanceCounters interface:

- unsigned getObjectCount() - returns number of objects (e.g. tables) containing non-zero performance counters
- unsigned getMaxCounterIndex() - returns maximum index number of the performance counters supported by the implementation (it's the same for all objects of the same group)
- unsigned getObjectId(unsigned index) - returns ID of the specified object
- const char* getObjectName(unsigned index) - returns name of the specified object
- const ISC_INT64* getObjectCounters(unsigned index) - returns pointer to the vector of performance counters (containing getMaxCounterIndex() + 1 elements) of the specified object

The returned pointer to the vector (as well as the whole instance of `PerformanceStats`) is valid until the object used to obtain the statistics (using the `getPerfStats()` method) is destroyed.
