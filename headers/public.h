#include <x86intrin.h>
#include <assert.h>

struct v2;
struct debug_scope_tree;

#define CAssert(condition) static_assert((condition), #condition )
#define Assert(condition) assert(condition)

