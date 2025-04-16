struct MemberDefinition
{
  String8 typeName;
  String8 fieldName;
  bool isPointer;
  bool isInUnion;
  u64 arrayCount;
};

struct MetaStruct
{
  String8 name;
  MemberDefinition *members;  
};

#include "generated.cpp"
