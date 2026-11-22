#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_fixed.h"
#include "bn_math.h"
#include "bn_algorithm.h"
#include "bn_vector.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_item.h"
#include "bn_regular_bg_ptr.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_text_generator.h"

#include "common_variable_8x16_sprite_font.h"
#include "bn_sprite_items_leonardo.h"
#include "bn_sprite_items_leonora.h"
#include "bn_sprite_items_portrait_leonardo.h"
#include "bn_sprite_items_portrait_leonora.h"
#include "bn_sprite_items_maria.h"
#include "bn_sprite_items_dialog_box.h"
#include "bn_regular_bg_items_alba.h"
#include "bn_regular_bg_items_interior.h"

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

    // Interior das casas: 16x16 tiles (256x256, centro em 0,0)
    // f chao  W parede  w janela  K estante  T mesa  b/B cama  P planta  X saida
    constexpr int in_size = 16;
    constexpr const char* in_rows[in_size] = {
        "                ",
        "                ",
        "                ",
        "                ",
        "  WWwWKKWWwWWW  ",
        "  fbffffffTTfP  ",
        "  fBffffffffff  ",
        "  ffffffffffff  ",
        "  ffffffffffff  ",
        "  ffffffffffff  ",
        "  ffffffffffff  ",
        "  fffffXXfffff  ",
        "                ",
        "                ",
        "                ",
        "                "
    };

    struct dialog_page
    {
        const char* l1;
        const char* l2;
    };

    constexpr dialog_page maria_dialog[3] = {
        { "Maria: Bom dia, Leonardo!", "Pronto para a aventura?" },
        { "Maria: Etruria e enorme!", "Vamos ver tudo juntos." },
        { "Maria: Mas antes, explore", "Alba com calma. Ate ja!" }
    };

    constexpr dialog_page books_dialog[2] = {
        { "Livros sobre a historia", "de Etruria e do Coliseu." },
        { "Um deles fala de uma", "estatua que ganha vida." }
    };

    constexpr dialog_page table_dialog[1] = {
        { "Um bilhete: Fui ao", "vinhedo. Volto logo!" }
    };

    constexpr dialog_page bed_dialog[1] = {
        { "Uma cama bem arrumada.", "Hoje nao e dia de dormir!" }
    };

    constexpr dialog_page plant_dialog[1] = {
        { "Uma planta bem cuidada.", "As folhas brilham." }
    };

    constexpr dialog_page window_dialog[1] = {
        { "Da janela se ve Alba e", "as colinas da Toscana." }
    };

    // frames do personagem: 0 baixo, 1 baixo2, 2 cima, 3 cima2, 4 esq, 5 esq andando A,
    // 6 esq andando B, 7 dir, 8 dir andando A, 9 dir andando B
    // facing: 0 baixo, 1 cima, 2 esquerda, 3 direita
    constexpr int frame_table[4][4] = {
        { 0, 1, 0, 1 },
        { 2, 3, 2, 3 },
        { 4, 5, 4, 6 },
        { 7, 8, 7, 9 }
    };

    char tile_at(bool inside, int x, int y)
    {
        int origin = inside ? 128 : 256;
        int size = inside ? in_size : map_size;
        int ax = x + origin;
        int ay = y + origin;

        if(ax < 0 || ay < 0)
        {
            return ' ';
        }

        int tx = ax / 16;
        int ty = ay / 16;

        if(tx >= size || ty >= size)
        {
            return ' ';
        }

        return inside ? in_rows[ty][tx] : map_rows[ty][tx];
    }

    bool blocked_char(bool inside, char c)
    {
        if(inside)
        {
            return c == ' ' || c == 'W' || c == 'w' || c == 'K' || c == 'T' ||
                    c == 'B' || c == 'b' || c == 'P';
        }

        return c == ' ' || c == 'C' || c == 'V' || c == 'R' || c == 'H' || c == 'D' || c == 'F';
    }

    bool can_move(bool inside, bn::fixed x, bn::fixed y, bn::fixed other_x, bn::fixed other_y)
    {
        if(! inside && bn::abs(x - other_x) < 12 && bn::abs(y - other_y) < 12)
        {
            return false;
        }

        int left = (x - 5).integer();
        int right = (x + 5).integer();
        int top = (y + 2).integer();
        int bottom = (y + 8).integer();

        return ! blocked_char(inside, tile_at(inside, left, top)) &&
                ! blocked_char(inside, tile_at(inside, right, top)) &&
                ! blocked_char(inside, tile_at(inside, left, bottom)) &&
                ! blocked_char(inside, tile_at(inside, right, bottom));
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

    // Escolha do personagem: 0 = Leonardo, 1 = Leonora
    int choice = 0;

    {
        bn::sprite_ptr portrait_m = bn::sprite_items::portrait_leonardo.create_sprite(-48, -8);
        bn::sprite_ptr portrait_f = bn::sprite_items::portrait_leonora.create_sprite(48, -8);
        bn::vector<bn::sprite_ptr, 24> header;
        bn::vector<bn::sprite_ptr, 12> name_m;
        bn::vector<bn::sprite_ptr, 12> name_f;
        bn::vector<bn::sprite_ptr, 16> hint;
        bn::vector<bn::sprite_ptr, 4> cursor;

        text_generator.generate(0, -66, "Escolha seu personagem", header);
        text_generator.generate(-48, 40, "Leonardo", name_m);
        text_generator.generate(48, 40, "Leonora", name_f);
        text_generator.generate(0, 72, "A: escolher", hint);

        bool changed = true;

        while(true)
        {
            if(bn::keypad::left_pressed())
            {
                choice = 0;
                changed = true;
            }
            else if(bn::keypad::right_pressed())
            {
                choice = 1;
                changed = true;
            }

            if(changed)
            {
                cursor.clear();
                text_generator.generate(choice == 0 ? -48 : 48, 56, "^", cursor);
                changed = false;
            }

            if(bn::keypad::a_pressed())
            {
                break;
            }

            bn::core::update();
        }
    }

    const bn::sprite_item& player_item = (choice == 0) ? bn::sprite_items::leonardo : bn::sprite_items::leonora;

    text_generator.set_left_alignment();
    text_generator.set_z_order(-2);

    // Mundo
    bn::camera_ptr camera = bn::camera_ptr::create(0, 0);
    bn::regular_bg_ptr bg_out = bn::regular_bg_items::alba.create_bg(0, 0);
    bn::regular_bg_ptr bg_in = bn::regular_bg_items::interior.create_bg(0, 0);
    bg_out.set_camera(camera);
    bg_in.set_camera(camera);
    bg_in.set_visible(false);

    const bn::fixed maria_x = 24;
    const bn::fixed maria_y = 40;
    bn::sprite_ptr maria = bn::sprite_items::maria.create_sprite(maria_x, maria_y - 8);
    maria.set_camera(camera);

    bn::fixed player_x = 8;
    bn::fixed player_y = 40;
    bn::fixed saved_x = 8;
    bn::fixed saved_y = 40;
    bn::sprite_ptr player = player_item.create_sprite(player_x, player_y - 8);
    player.set_camera(camera);

    bool inside = false;
    int facing = 0;
    int shown_frame = 0;
    int anim = 0;

    bn::vector<bn::sprite_ptr, 4> box;
    bn::vector<bn::sprite_ptr, 32> line1;
    bn::vector<bn::sprite_ptr, 32> line2;
    bool in_dialog = false;
    const dialog_page* dialog = nullptr;
    int dialog_size = 0;
    int page = 0;

    auto open_dialog = [&](const dialog_page* pages, int count)
    {
        dialog = pages;
        dialog_size = count;
        page = 0;
        in_dialog = true;
        box.clear();
        line1.clear();
        line2.clear();

        for(int index = 0; index < 4; ++index)
        {
            bn::sprite_ptr piece = bn::sprite_items::dialog_box.create_sprite(-96 + index * 64, 56);
            piece.set_z_order(-1);
            box.push_back(piece);
        }

        text_generator.generate(-112, 48, dialog[0].l1, line1);
        text_generator.generate(-112, 64, dialog[0].l2, line2);
    };

    while(true)
    {
        if(in_dialog)
        {
            if(bn::keypad::a_pressed())
            {
                ++page;
                line1.clear();
                line2.clear();

                if(page >= dialog_size)
                {
                    in_dialog = false;
                    box.clear();
                }
                else
                {
                    text_generator.generate(-112, 48, dialog[page].l1, line1);
                    text_generator.generate(-112, 64, dialog[page].l2, line2);
                }
            }
        }
        else
        {
            bn::fixed dx = 0;
            bn::fixed dy = 0;
            bool moving = true;

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
            else
            {
                moving = false;
            }

            if(moving)
            {
                ++anim;
            }
            else
            {
                anim = 0;
            }

            int frame = frame_table[facing][(anim / 6) % 4];

            if(frame != shown_frame)
            {
                player.set_tiles(player_item.tiles_item(), frame);
                shown_frame = frame;
            }

            if(! inside && dy < 0 && tile_at(false, player_x.integer(), (player_y - 8).integer()) == 'D')
            {
                // Entrar na casa
                saved_x = player_x;
                saved_y = player_y;
                inside = true;
                bg_out.set_visible(false);
                bg_in.set_visible(true);
                maria.set_visible(false);
                player_x = -8;
                player_y = 24;
                facing = 1;
                anim = 0;
            }
            else
            {
                if(dx != 0 && can_move(inside, player_x + dx, player_y, maria_x, maria_y))
                {
                    player_x += dx;
                }

                if(dy != 0 && can_move(inside, player_x, player_y + dy, maria_x, maria_y))
                {
                    player_y += dy;
                }

                if(inside && tile_at(true, player_x.integer(), (player_y + 8).integer()) == 'X')
                {
                    // Sair da casa
                    inside = false;
                    bg_in.set_visible(false);
                    bg_out.set_visible(true);
                    maria.set_visible(true);
                    player_x = saved_x;
                    player_y = saved_y;
                    facing = 0;
                    anim = 0;
                }
            }

            player.set_position(player_x, player_y - 8);

            if(bn::keypad::a_pressed())
            {
                if(! inside)
                {
                    bn::fixed ddx = player_x - maria_x;
                    bn::fixed ddy = player_y - maria_y;

                    if(bn::abs(ddx) < 28 && bn::abs(ddy) < 28)
                    {
                        int maria_frame = 0;

                        if(bn::abs(ddx) > bn::abs(ddy))
                        {
                            maria_frame = ddx < 0 ? 2 : 3;
                        }
                        else
                        {
                            maria_frame = ddy < 0 ? 1 : 0;
                        }

                        maria.set_tiles(bn::sprite_items::maria.tiles_item(), maria_frame);
                        open_dialog(maria_dialog, 3);
                    }
                }
                else
                {
                    int fdx = (facing == 2) ? -1 : ((facing == 3) ? 1 : 0);
                    int fdy = (facing == 0) ? 1 : ((facing == 1) ? -1 : 0);
                    char c = tile_at(true, player_x.integer() + fdx * 12, player_y.integer() + 4 + fdy * 12);

                    if(c == 'K')
                    {
                        open_dialog(books_dialog, 2);
                    }
                    else if(c == 'T')
                    {
                        open_dialog(table_dialog, 1);
                    }
                    else if(c == 'B' || c == 'b')
                    {
                        open_dialog(bed_dialog, 1);
                    }
                    else if(c == 'P')
                    {
                        open_dialog(plant_dialog, 1);
                    }
                    else if(c == 'w')
                    {
                        open_dialog(window_dialog, 1);
                    }
                }
            }
        }

        if(inside)
        {
            camera.set_position(0, 0);
        }
        else
        {
            bn::fixed camera_x = bn::clamp(player_x, bn::fixed(-136), bn::fixed(136));
            bn::fixed camera_y = bn::clamp(player_y, bn::fixed(-176), bn::fixed(176));
            camera.set_position(camera_x, camera_y);
        }

        bn::core::update();
    }
}
