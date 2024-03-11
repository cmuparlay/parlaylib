licenses(["notice"])

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "acquire_retire",
    hdrs = ["acquire_retire.h"],
    deps = [
        "//parlay:portability",
        "//parlay:thread_specific",
    ],
)

cc_library(
    name = "hazptr_stack",
    hdrs = ["hazptr_stack.h"],
    deps = [
        ":acquire_retire",
    ],
)
