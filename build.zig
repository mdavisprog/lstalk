const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const c_flags: []const []const u8 = &.{
        "-std=c99",
        "-Wall",
        "-Wextra",
        "-Werror",
    };

    const lib_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });
    lib_module.addCSourceFiles(.{
        .files = &.{
            "src/lib/lstalk.c",
        },
        .flags = c_flags,
    });

    const test_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
        .imports = &.{
            .{ .name = "lib", .module = lib_module },
        },
    });
    test_module.addCSourceFiles(.{
        .files = &.{
            "src/test/main.c",
        },
        .flags = c_flags,
    });

    const test_exe = b.addExecutable(.{
        .name = "test",
        .root_module = test_module,
    });

    b.installArtifact(test_exe);

    const test_step = b.step("test", "Run test application");
    const test_cmd = b.addRunArtifact(test_exe);
    test_step.dependOn(&test_cmd.step);
    test_cmd.step.dependOn(b.getInstallStep());
}
