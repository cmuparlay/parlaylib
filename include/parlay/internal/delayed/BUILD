licenses(["notice"])

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "common",
    hdrs = ["common.h"],
    deps = [
        "//parlay:range",
    ],
)

cc_library(
    name = "filter",
    hdrs = ["filter.h"],
    deps = [
        ":common",
        ":flatten",
        ":map",
        "//parlay:range",
        "//parlay:relocation",
        "//parlay:sequence",
        "//parlay:utilities",
        "//parlay/internal:sequence_ops",
        "//parlay/internal:uninitialized_sequence",
    ],
)

cc_library(
    name = "filter_op",
    hdrs = ["filter_op.h"],
    deps = [
        ":common",
        ":flatten",
        "//parlay:range",
        "//parlay:relocation",
        "//parlay:sequence",
        "//parlay:utilities",
        "//parlay/internal:sequence_ops",
        "//parlay/internal:uninitialized_sequence",
    ],
)

cc_library(
    name = "flatten",
    hdrs = ["flatten.h"],
    deps = [
        ":common",
        ":map",
        ":terminal",
        "//parlay:monoid",
        "//parlay:parallel",
        "//parlay:range",
        "//parlay:sequence",
        "//parlay:slice",
        "//parlay:type_traits",
        "//parlay:utilities",
        "//parlay/internal:sequence_ops",
    ],
)

cc_library(
    name = "map",
    hdrs = ["map.h"],
    deps = [
        ":common",
        "//parlay:range",
        "//parlay:type_traits",
        "//parlay:utilities",
        "//parlay/internal:sequence_ops",
    ],
)

cc_library(
    name = "scan",
    hdrs = ["scan.h"],
    deps = [
        ":common",
        "//parlay:monoid",
        "//parlay:parallel",
        "//parlay:range",
        "//parlay:sequence",
        "//parlay:slice",
        "//parlay:type_traits",
        "//parlay:utilities",
        "//parlay/internal:sequence_ops",
    ],
)

cc_library(
    name = "terminal",
    hdrs = ["terminal.h"],
    deps = [
        ":common",
        "//parlay:monoid",
        "//parlay:parallel",
        "//parlay:range",
        "//parlay:sequence",
        "//parlay:slice",
        "//parlay/internal:sequence_ops",
    ],
)

cc_library(
    name = "zip",
    hdrs = ["zip.h"],
    deps = [
        ":common",
        "//parlay:range",
        "//parlay:type_traits",
        "//parlay/internal:sequence_ops",
    ],
)
