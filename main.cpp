#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_fixed.h"
#include "bn_math.h"
#include "bn_algorithm.h"
#include "bn_vector.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_regular_bg_ptr.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_text_generator.h"

#include "common_variable_8x16_sprite_font.h"
#include "bn_sprite_items_leonardo.h"
#include "bn_sprite_items_maria.h"
#include "bn_sprite_items_dialog_box.h"
#include "bn_regular_bg_items_alba.h"

namespace
{
    // Mapa de Alba: 32x32 tiles de 16x16 pixels (mundo de 512x512, centro em 0,0)
    // . grama   = estrada   o praca   C cipreste   S girassois   V vinhedo
    // R telhado H parede    D porta   F fonte
    constexpr int map_size = 32;
    constexpr const char* map_rows[map_size] = {
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC",
        "C..............==..............C",
        "C............C.==.C............C",
        "C..............==..............C",
        "C..RRR.......C.==.C......RRR...C",
        "C..HDH.........==........HDH...C",
        "C...=..VVVVVVC.==.CSSSSSS.=....C",
        "C...=..VVVVVV..==..SSSSSS.=....C",
        "C...=..VVVVVVC.==.CSSSSSS.=....C",
        "C...=..VVVVVV..==..SSSSSS.=....C",
        "C...=..VVVVVVC.==.CSSSSSS.=....C",
        "C...=..VVVVVV..==..SSSSSS.=....C",
        "C...=..VVVVVVC.==.CSSSSSS.=....C",
        "C..C=C.C.C.C.oooooo..C.C.C=C.C.C",
        "C...=........oooooo.......=....C",
        "C============ooFFoo============C",
        "C============ooFFoo============C",
        "C............oooooo............C",
        "C..C.C.C.C.C.oooooo..C.C.C.C.C.C",
        "C..............==..............C",
        "C..VVVVVVVVVVC.==.CSSSSSSSSSS..C",
        "C..VVVVVVVVVV..==..SSSSSSSSSS..C",
        "C..VVVVVVVVVVC.==.CSSSSSSSSSS..C",
        "C..VVVVVVVVVV..==..SSSSSSSSSS..C",
        "C..VVVVVVVVVVC.==.CSSSSSSSSSS..C",
        "C..VVVVVVVVVV..==..SSSSSSSSSS..C",
        "C..VVVVVVVVVVC.==.CSSSSSSSSSS..C",
        "C..VVVVVVVVVV..==..SSSSSSSSSS..C",
        "C..VVVVVVVVVVC.==.CSSSSSSSSSS..C",
        "C..............==..............C",
        "C..............==..............C",
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
    };

    constexpr int maria_dialog_size = 3;
    constexpr const char* maria_dialog[maria_dialog_size][2] = {
        { "Maria: Bom dia, Leonardo!", "Pronto para a aventura?" },
        { "Maria: Etruria e enorme!", "Vamos ver tudo juntos." },
        { "Maria: Mas antes, explore", "Alba com calma. Ate ja!" }
    };

    bool is_blocked(int x, int y)
    {
        int ax = x + 256;
        int ay = y + 256;

        if(ax < 0 || ay < 0)
        {
            return true;
        }

        int tx = ax / 16;
        int ty = ay / 16;

        if(tx >= map_size || ty >= map_size)
        {
            return true;
        }

        char c = map_rows[ty][tx];
        return c == 'C' || c == 'V' || c == 'R' || c == 'H' || c == 'D' || c == 'F';
    }

    bool can_move(bn::fixed x, bn::fixed y, bn::fixed other_x, bn::fixed other_y)
    {
        if(bn::abs(x - other_x) < 12 && bn::abs(y - other_y) < 12)
        {
            return false;
        }

        int left = (x - 5).integer();
        int right = (x + 5).integer();
        int top = (y + 2).integer();
        int bottom = (y + 8).integer();

        return ! is_blocked(left, top) && ! is_blocked(right, top) &&
                ! is_blocked(left, bottom) && ! is_blocked(right, bottom);
    }
}

int main()
{
    bn::core::init();
    bn::bg_palettes::set_transparent_color(bn::color(4, 3, 8));

    bn::sprite_text_generator text_generator(common::variable_8x16_sprite_font);
    text_generator.set_center_alignment();

    // Tela de titulo
    {
        bn::vector<bn::sprite_ptr, 16> title;
        bn::vector<bn::sprite_ptr, 24> prompt;
        text_generator.generate(0, -16, "ETRURIA", title);
        text_generator.generate(0, 24, "Aperte START", prompt);

        while(! bn::keypad::start_pressed())
        {
            bn::core::update();
        }
    }

    text_generator.set_left_alignment();
    text_generator.set_z_order(-2);

    // Cidade de Alba
    bn::camera_ptr camera = bn::camera_ptr::create(0, 0);
    bn::regular_bg_ptr bg = bn::regular_bg_items::alba.create_bg(0, 0);
    bg.set_camera(camera);

    const bn::fixed maria_x = 24;
    const bn::fixed maria_y = 40;
    bn::sprite_ptr maria = bn::sprite_items::maria.create_sprite(maria_x, maria_y);
    maria.set_camera(camera);

    bn::fixed player_x = 8;
    bn::fixed player_y = 40;
    bn::sprite_ptr player = bn::sprite_items::leonardo.create_sprite(player_x, player_y);
    player.set_camera(camera);

    int facing = 0;
    int shown_facing = 0;

    bn::vector<bn::sprite_ptr, 4> box;
    bn::vector<bn::sprite_ptr, 32> line1;
    bn::vector<bn::sprite_ptr, 32> line2;
    bool in_dialog = false;
    int page = 0;

    while(true)
    {
        if(in_dialog)
        {
            if(bn::keypad::a_pressed())
            {
                ++page;
                line1.clear();
                line2.clear();

                if(page >= maria_dialog_size)
                {
                    in_dialog = false;
                    box.clear();
                }
                else
                {
                    text_generator.generate(-112, 48, maria_dialog[page][0], line1);
                    text_generator.generate(-112, 64, maria_dialog[page][1], line2);
                }
            }
        }
        else
        {
            bn::fixed dx = 0;
            bn::fixed dy = 0;

            if(bn::keypad::left_held())
            {
                dx = -1;
                facing = 2;
            }
            else if(bn::keypad::right_held())
            {
                dx = 1;
                facing = 3;
            }
            else if(bn::keypad::up_held())
            {
                dy = -1;
                facing = 1;
            }
            else if(bn::keypad::down_held())
            {
                dy = 1;
                facing = 0;
            }

            if(facing != shown_facing)
            {
                player.set_tiles(bn::sprite_items::leonardo.tiles_item(), facing);
                shown_facing = facing;
            }

            if(dx != 0 && can_move(player_x + dx, player_y, maria_x, maria_y))
            {
                player_x += dx;
            }

            if(dy != 0 && can_move(player_x, player_y + dy, maria_x, maria_y))
            {
                player_y += dy;
            }

            player.set_position(player_x, player_y);

            if(bn::keypad::a_pressed() &&
                    bn::abs(player_x - maria_x) < 28 && bn::abs(player_y - maria_y) < 28)
            {
                in_dialog = true;
                page = 0;

                for(int index = 0; index < 4; ++index)
                {
                    bn::sprite_ptr piece = bn::sprite_items::dialog_box.create_sprite(-96 + index * 64, 56);
                    piece.set_z_order(-1);
                    box.push_back(piece);
                }

                text_generator.generate(-112, 48, maria_dialog[0][0], line1);
                text_generator.generate(-112, 64, maria_dialog[0][1], line2);
            }
        }

        bn::fixed camera_x = bn::clamp(player_x, bn::fixed(-136), bn::fixed(136));
        bn::fixed camera_y = bn::clamp(player_y, bn::fixed(-176), bn::fixed(176));
        camera.set_position(camera_x, camera_y);

        bn::core::update();
    }
}
