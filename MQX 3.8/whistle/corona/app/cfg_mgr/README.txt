Generating new WMP .c .h files

OS X:
1. Generate pb file:
protoc -I../../common/nanopb/generator -I. -I..\..\..\..\..\utils\protobuf\protobuf-2.5.0\src\ -o cfg_mgr_dynamic.pb cfg_mgr_dynamic.proto
2. Generate c files:
python ../../common/nanopb/generator/nanopb_generator.py cfg_mgr_dynamic.pb



Inspecting generated content, use protobuf's protoc utility
protoc --decode=AccelData WhistleMessage.proto < /tmp/tempdecode

protoc --decode_raw < /tmp/thefiletoparse