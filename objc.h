// Tiny helpers for talking to Objective-C frameworks from plain C.
//
// Every Objective-C method call `[obj foo:x]` compiles to
// `objc_msgSend(obj, @selector(foo:), x)`. objc_msgSend has no real prototype,
// so we must cast it to the exact function type of the method before calling:
//
//   MSG(ret, argtypes...)(obj, sel("foo:"), args...)
//
// On arm64 there is no objc_msgSend_stret/_fpret, so this cast works for
// every return type, including structs like CGRect.
#ifndef ZOOMER_OBJC_H
#define ZOOMER_OBJC_H

#include <objc/message.h>
#include <objc/runtime.h>

#define MSG(ret, ...) ((ret(*)(id, SEL, ##__VA_ARGS__))objc_msgSend)

#define cls(name) ((id)objc_getClass(name))
#define sel(name) sel_registerName(name)

// [[Class alloc] init]
#define NEW(name) MSG(id)(MSG(id)(cls(name), sel("alloc")), sel("init"))

static inline id nsstr(const char *s) {
  return MSG(id, const char *)(cls("NSString"), sel("stringWithUTF8String:"), s);
}

static inline const char *cstr(id nsstring) {
  return MSG(const char *)(nsstring, sel("UTF8String"));
}

#endif
