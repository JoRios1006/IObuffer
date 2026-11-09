set_project("iobuffer")
set_version("0.1.0")
set_license("GPL-2.0-or-later")
set_xmakever("2.8.0")
set_languages("c++17")
set_warnings("all", "extra")
add_rules("mode.debug", "mode.release")

-- Librería header-only: no se compila, solo expone include/ y se instala.
target("iobuffer")
    set_kind("headeronly")
    add_includedirs("include", {public = true}) -- quien haga add_deps("iobuffer") hereda este include
    add_headerfiles("include/(iobuffer/*.hpp)") -- se instala como <prefix>/include/iobuffer/*.hpp
target_end()

-- Tests: `xmake test` los construye y corre. No se compilan en un `xmake` normal.
target("test_iobuffer")
    set_kind("binary")
    set_default(false)
    add_files("tests/test_iobuffer.cpp")
    add_deps("iobuffer")
    add_syslinks("pthread")
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
    add_tests("default")
target_end()
