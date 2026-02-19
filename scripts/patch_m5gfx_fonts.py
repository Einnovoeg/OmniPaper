Import("env")

from pathlib import Path


def _replace_once(data: str, needle: str, replacement: str) -> str:
    if needle not in data:
        return data
    return data.replace(needle, replacement, 1)


def patch_fonts_cpp(path: Path) -> bool:
    data = path.read_text()
    updated = data

    if "LGFX_DISABLE_JAPANESE_FONTS" not in updated:
        include_block = (
            '#include "../Fonts/IPA/lgfx_font_japan.h"\n'
            '#include "../Fonts/efont/lgfx_efont_cn.h"\n'
            '#include "../Fonts/efont/lgfx_efont_ja.h"\n'
            '#include "../Fonts/efont/lgfx_efont_kr.h"\n'
            '#include "../Fonts/efont/lgfx_efont_tw.h"\n'
        )
        if include_block in updated:
            updated = updated.replace(
                include_block,
                "#ifndef LGFX_DISABLE_JAPANESE_FONTS\n"
                "#include \"../Fonts/IPA/lgfx_font_japan.h\"\n"
                "#endif\n"
                "#ifndef LGFX_DISABLE_EFONT\n"
                "#include \"../Fonts/efont/lgfx_efont_cn.h\"\n"
                "#include \"../Fonts/efont/lgfx_efont_ja.h\"\n"
                "#include \"../Fonts/efont/lgfx_efont_kr.h\"\n"
                "#include \"../Fonts/efont/lgfx_efont_tw.h\"\n"
                "#endif\n",
            )

        updated = _replace_once(
            updated,
            "    const U8g2font lgfxJapanMincho_8   = { lgfx_font_japan_mincho_8    };\n",
            "    #ifndef LGFX_DISABLE_JAPANESE_FONTS\n"
            "    const U8g2font lgfxJapanMincho_8   = { lgfx_font_japan_mincho_8    };\n",
        )
        updated = _replace_once(
            updated,
            "    const U8g2font lgfxJapanGothicP_40 = { lgfx_font_japan_gothic_p_40 };\n\n",
            "    const U8g2font lgfxJapanGothicP_40 = { lgfx_font_japan_gothic_p_40 };\n"
            "    #endif\n\n",
        )

    if "LGFX_DISABLE_EFONT" not in updated:
        updated = _replace_once(
            updated,
            "    const U8g2font efontCN_10     = { lgfx_efont_cn_10    };\n",
            "    #ifndef LGFX_DISABLE_EFONT\n"
            "    const U8g2font efontCN_10     = { lgfx_efont_cn_10    };\n",
        )
        updated = _replace_once(
            updated,
            "    const U8g2font efontTW_24_i   = { lgfx_efont_tw_24_i  };\n",
            "    const U8g2font efontTW_24_i   = { lgfx_efont_tw_24_i  };\n"
            "    #endif\n",
        )

    if updated != data:
        path.write_text(updated)
        return True
    return False


def patch_fonts_hpp(path: Path) -> bool:
    data = path.read_text()
    updated = data

    if "LGFX_DISABLE_JAPANESE_FONTS" not in updated:
        updated = _replace_once(
            updated,
            "    extern const lgfx::U8g2font lgfxJapanMincho_8  ;\n",
            "    #ifndef LGFX_DISABLE_JAPANESE_FONTS\n"
            "    extern const lgfx::U8g2font lgfxJapanMincho_8  ;\n",
        )
        updated = _replace_once(
            updated,
            "    extern const lgfx::U8g2font lgfxJapanGothicP_40;\n\n",
            "    extern const lgfx::U8g2font lgfxJapanGothicP_40;\n"
            "    #endif\n\n",
        )

    if "LGFX_DISABLE_EFONT" not in updated:
        updated = _replace_once(
            updated,
            "    extern const lgfx::U8g2font efontCN_10   ;\n",
            "    #ifndef LGFX_DISABLE_EFONT\n"
            "    extern const lgfx::U8g2font efontCN_10   ;\n",
        )
        updated = _replace_once(
            updated,
            "    extern const lgfx::U8g2font efontTW_24_i ;\n",
            "    extern const lgfx::U8g2font efontTW_24_i ;\n"
            "    #endif\n",
        )

    if updated != data:
        path.write_text(updated)
        return True
    return False


def main() -> None:
    libdeps_dir = Path(env["PROJECT_LIBDEPS_DIR"]) / env["PIOENV"] / "M5GFX" / "src" / "lgfx" / "v1"
    if not libdeps_dir.exists():
        return

    fonts_cpp = libdeps_dir / "lgfx_fonts.cpp"
    fonts_hpp = libdeps_dir / "lgfx_fonts.hpp"
    changed = False

    if fonts_cpp.exists():
        changed |= patch_fonts_cpp(fonts_cpp)
    if fonts_hpp.exists():
        changed |= patch_fonts_hpp(fonts_hpp)

    if changed:
        print("Patched M5GFX font sources to allow disabling CJK fonts.")


main()
