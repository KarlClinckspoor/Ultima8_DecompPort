// mingw builds of libmatoya: the type WebView2's header wants, which mingw-w64 lacks.
#pragma once
typedef struct EventRegistrationToken { __int64 value; } EventRegistrationToken;
