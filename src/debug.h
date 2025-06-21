
#ifdef _DEBUG
#include <assert.h>	// assert()
#define DEBUG_ASSERT(exp) assert(exp)
void debug_printf(ES_UTF8 *format,...);
void debug_error_printf(const ES_UTF8 *format,...);
#else
#define DEBUG_ASSERT(exp)
#define debug_printf(format,...)
#define debug_error_printf(format,...)
#endif

