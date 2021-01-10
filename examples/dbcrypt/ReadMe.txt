**************************************************************************************
*				All files in this directory are trivial samples.					 *
* They do not perform any real data encryption and should not be used in production! *
**************************************************************************************

Brief description of the sample.

Sample contains 3 components - fbSampleDbCrypt plugin, fbSampleKeyHolder plugin and application,
which can pass crypt key to server. Plugins do not perform any real encryption (XOR with single
byte hardly can be treated as encryption) though makes database useless without crypt plugin,
key is sent between components in plain form - they just demonstrate what calls in plugins
should be done and what methods should be implemented in order for plugin to start to work.

Depending upon settings in configuration file plugins may use different ways to manage encryption
key. fbSampleDbCrypt's configuration file may contain following parameters:
Auto - boolean value, when FALSE plugin queries KeyHolder plugin for key value (this is default),
	when TRUE get key value from "Value" configuration parameter.
Value - integer value (lower byte is actually used), used in "Auto" mode as key value (default 90).

fbSampleKeyHolder's configuration file may contain following parameters:
Auto - boolean value, when FALSE plugin queries client application for key value (this is default),
	when TRUE get key value from configuration file by name or use default (90) for unnamed key.
Key{Name} - integer value, a key with name "Name" (i.e. when one issues "ALTER DATABASE ENCRYPT ...
	KEY Doggy" configuration parameter KeyDoggy should be present).
OnlyOwnKey - boolean value, enables/disables use of a key from another key holder in SuperServer.
	Default value is TRUE (i.e. only key, owned by this KeyHolder, can be used by related
	attachment).

Crypt application has a few parameters making it possible to demonstrate different operations.
-e - Encrypt database (use gstat to monitor crypt progress).
-d - Decrypt database.
-l - Locally execute SELECT statement returning name of currently attached user.
-r - Execute same statement using remote datasource 'localhost:employee'. To make it work
	 user "test" with password "test" should be created in employee database. If employee was
	 encrypted in advance this demonstrates passing database crypt key through the chain of
	 key holders.

cryptDb.pas is a minimum (XOR using fixed key hardcoded in plugin body) sample of database crypt
plugin written on Pascal. Was tested with both FreePascal and Delphi.
