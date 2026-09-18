#pragma once
#include <cassert>
#include <cstdio>

// Verify actual visible labels, including dialogs and dropdown popups. Checking
// symbols.txt alone cannot detect a UI character accidentally omitted from it.
inline void assert_visible_glyphs(lv_obj_t *obj)
{
    if(lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
    if(lv_obj_check_type(obj, &lv_label_class)) {
        const auto *font=lv_obj_get_style_text_font(obj, LV_PART_MAIN);
        const auto *text=reinterpret_cast<const unsigned char *>(lv_label_get_text(obj));
        while(*text) {
            uint32_t codepoint=*text++;
            unsigned remaining=0;
            if(codepoint>=0xf0) { codepoint&=7;remaining=3; }
            else if(codepoint>=0xe0) { codepoint&=15;remaining=2; }
            else if(codepoint>=0xc0) { codepoint&=31;remaining=1; }
            while(remaining--) { assert((*text&0xc0)==0x80);codepoint=(codepoint<<6)|(*text++&63); }
            if(codepoint<32) continue;
            lv_font_glyph_dsc_t glyph={};
            const bool present=lv_font_get_glyph_dsc(font, &glyph, codepoint, 0) && !glyph.is_placeholder;
            if(!present) std::fprintf(stderr,"Missing UI glyph U+%04lx in %s\n",static_cast<unsigned long>(codepoint),lv_label_get_text(obj));
            assert(present);
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(obj);++i) assert_visible_glyphs(lv_obj_get_child(obj,i));
}
