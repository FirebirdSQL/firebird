Object Pascal Examples for Firebird
===================================


The aim of these examples is to copy as closely as possible the equivalent
C++ examples. The primary intention is to demonstrate how to make calls to 
the new Firebird OO API. For this reason they should not be considered 
examples of best practice.

Project files for Delphi and Lazarus are not supplied. Project files take 
up a lot of space and hide the simplicity of the sample programs. 

Each sample file can easily be converted into a project and the way to do
this is explained below.


Requirements
------------

The examples have been tested with the Free Pascal compiler on linux. 
The code should work with Delphi but has not been tested.


Building and running the examples
---------------------------------

Just type make at the command line.
If you do not have a standard firebird installation be sure to change
the variables at the top of the make file.


Opening the code in Lazarus or Delphi
-------------------------------------

Each example is a stand-alone program. To open and run it in your favourite 
Object Pascal IDE you just have to convert the example to a project. In Lazarus
you would do the following to create a project from 03.select.pas:

 - Copy 03.select.pas and rename it select.lpr
 - Open select.lpr as a project
 - When prompted choose 'Simple Program' as the project template
 - Go into Project options and add the following paths:
    /opt/firebird/include/Firebird
    common

You can then compile and run the example through the debugger.


