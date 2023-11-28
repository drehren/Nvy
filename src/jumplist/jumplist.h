struct JumpList;

static constexpr wchar_t APP_ID[] = L"RMichelsen.Nvy";

JumpList *JumpListInit();
void JumpListDestroy(JumpList **jl);

void JumpListAdd(JumpList *jl, mpack_node_t path_node);
