#include "ui/ui_widget.h"

// 160 * 80

int cnts[9] = {0};

static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        const char *text = lv_label_get_text(label);

        int id = text[6] - '0';
        cnts[id]++;
        lv_label_set_text_fmt(label, "Button%d: %d", id, cnts[id]);
    }
}

void ui_init(void)
{

    static int32_t col_array[3] = {30, 30};
    static int32_t row_array[3] = {20, 20};
    static int32_t col_dsc[2], row_dsc[2];
    grid_dsc_array(col_dsc, col_array, 2);
    grid_dsc_array(row_dsc, row_array, 2);
    static lv_style_t style;
    lv_style_init(&style);

    /*Set a background color and a radius*/
    lv_style_set_radius(&style, 5 * SCALE);
    lv_style_set_bg_opa(&style, LV_OPA_COVER);
    lv_style_set_bg_color(&style, lv_palette_lighten(LV_PALETTE_GREEN, 1));

    /*Add a shadow*/
    lv_style_set_shadow_width(&style, 5 * SCALE);
    lv_style_set_shadow_color(&style, lv_palette_main(LV_PALETTE_BLUE));

    /*Create a container with grid*/
    lv_obj_t *cont = lv_obj_create(lv_screen_active());
    lv_obj_set_style_grid_column_dsc_array(cont, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(cont, row_dsc, 0);
    obj_set_size(cont, 110, 80);

    lv_obj_set_align(cont, LV_ALIGN_TOP_LEFT);
    lv_obj_set_layout(cont, LV_LAYOUT_GRID);

    // lv_obj_t *btns[9];
    // lv_obj_t *btn = lv_button_create(cont);
    // lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, 1, 1,
    //                      LV_GRID_ALIGN_STRETCH, 1, 1);
    // lv_obj_t *label = lv_label_create(btn);

    // for (int i = 0; i < 9; i++)
    // {
    //     uint8_t col = i % 3;
    //     uint8_t row = i / 3;
    //     if (i == 8)
    //         break;
    //     btn = lv_button_create(cont); /*Add a button the current screen*/ /*Set its size*/
    //     // lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);       /*Assign a callback to the button*/
    //     lv_obj_set_grid_cell(btn, LV_GRID_ALIGN_STRETCH, col, 1,
    //                          LV_GRID_ALIGN_STRETCH, row, 1);

    //     // label = lv_label_create(btn); /*Add a label to the button*/
    //     // lv_label_set_text_fmt(label, "button%d", i);
    //     // lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN); /*Set the labels text*/
    //     // lv_obj_center(label);

    //     lv_obj_add_style(btn, &style, 0);
    // }

    // // lv_obj_t * svg = lv_image_create(cont);
    // // lv_image_set_src(svg, "/res/svg/email.svg");
    // // lv_image_set_inner_align(svg, LV_ALIGN_CENTER);
    // // lv_obj_set_grid_cell(svg,LV_GRID_ALIGN_STRETCH,2,1,
    // //     LV_GRID_ALIGN_STRETCH, 2, 1);

    /*Create a LED and switch it OFF*/
    lv_obj_t *led1 = lv_led_create(lv_screen_active());
    obj_set_pos(led1, 120, 20);
    obj_set_size(led1, 5, 5);
    // lv_obj_align(led1, LV_ALIGN_CENTER, -80, 0);
    lv_led_off(led1);

    /*Copy the previous LED and set a brightness*/
    lv_obj_t *led2 = lv_led_create(lv_screen_active());
    obj_set_pos(led2, 120, 40);
    obj_set_size(led2, 5, 5);
    // lv_obj_align(led2, LV_ALIGN_CENTER, 0, 0);
    lv_led_set_brightness(led2, 150);
    lv_led_set_color(led2, lv_palette_main(LV_PALETTE_RED));

    /*Copy the previous LED and switch it ON*/
    lv_obj_t *led3 = lv_led_create(lv_screen_active());
    obj_set_pos(led3, 120, 60);
    obj_set_size(led3, 5, 5);
    // lv_obj_align(led3, LV_ALIGN_CENTER, 80, 0);
    lv_led_on(led3);
}
