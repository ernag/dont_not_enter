protoc -o whistlemessage.pb WhistleMessage.proto
python ../nanopb/generator/nanopb_generator.py whistlemessage.pb
move whistlemessage.pb.h ..\..\include