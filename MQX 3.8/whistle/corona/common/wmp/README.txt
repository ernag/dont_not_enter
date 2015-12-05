https://whistle.atlassian.net/wiki/display/COR/EVTMGR%3A+Adding+a+New+Event

INSTALLATION

WINDOWS:
Download proto exe from https://code.google.com/p/protobuf/downloads/list and put it in your path
Install python and put pip in your path
Install python protobuf via pip: pip install protobuf



Generating new WMP .c .h files

OS X:
1. Generate pb file:
protoc -o whistlemessage.pb WhistleMessage.proto
2. Generate c files:
python ../nanopb/generator/nanopb_generator.py whistlemessage.pb
move whistlemessage.pb.h ..\..\include

Windows:
1. Generate pb file:
protoc -o whistlemessage.pb WhistleMessage.proto
2. Generate c files:
python ../nanopb/generator/nanopb_generator.py whistlemessage.pb
3. Move the header file to include directory:
move whistlemessage.pb.h ..\..\include

Inspecting generated content, use protobuf's protoc utility
protoc --decode=AccelData WhistleMessage.proto < /tmp/tempdecode

protoc --decode_raw < /tmp/thefiletoparse



protoc -o datamessage.pb DataMessage.proto
python ..\nanopb\generator\nanopb_generator.py datamessage.pb
move datamessage.pb.h ..\..\include
