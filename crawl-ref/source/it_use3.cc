/*
 *  File:       it_use3.cc
 *  Summary:    Functions for using some of the wackier inventory items.
 *  Written by: Linley Henzell
 */

#include "AppHdr.h"

#include "it_use3.h"

#include <algorithm>
#include <cstdlib>
#include <string.h>

#include "externs.h"

#include "artefact.h"
#include "beam.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "decks.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "fight.h"
#include "food.h"
#include "invent.h"
#include "items.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "mapmark.h"
#include "message.h"
#include "mon-place.h"
#include "mon-util.h"
#include "mgen_data.h"
#include "coord.h"
#include "misc.h"
#include "player.h"
#include "player-stats.h"
#include "religion.h"
#include "godconduct.h"
#include "skills.h"
#include "skills2.h"
#include "spells1.h"
#include "spells2.h"
#include "spl-book.h"
#include "spl-cast.h"
#include "state.h"
#include "stuff.h"
#include "areas.h"
#include "view.h"
#include "shout.h"
#include "xom.h"

// TODO: Let artefacts besides weapons generate noise.
void noisy_equipment()
{
    if (silenced(you.pos()) || !one_chance_in(20))
        return;

    std::string msg;

    const item_def* weapon = you.weapon();

    if (weapon && is_unrandom_artefact(*weapon))
    {
        std::string name = weapon->name(DESC_PLAIN, false, true, false, false,
                                        ISFLAG_IDENT_MASK);
        msg = getSpeakString(name.c_str());
        if (!msg.empty())
        {
            // "Your Singing Sword" sounds disrespectful
            // (as if there could be more than one!)
            msg = replace_all(msg, "@Your_weapon@", "@The_weapon@");
            msg = replace_all(msg, "@your_weapon@", "@the_weapon@");
        }
    }

    if (msg.empty())
    {
        msg = getSpeakString("noisy weapon");
        if (!msg.empty())
        {
            msg = replace_all(msg, "@Your_weapon@", "Your @weapon@");
            msg = replace_all(msg, "@your_weapon@", "your @weapon@");
        }
    }

    // Set appropriate channel (will usually be TALK).
    msg_channel_type channel = MSGCH_TALK;

    // Disallow anything with VISUAL in it.
    if (!msg.empty() && msg.find("VISUAL") != std::string::npos)
        msg.clear();

    if (!msg.empty())
    {
        std::string param;
        const std::string::size_type pos = msg.find(":");

        if (pos != std::string::npos)
            param = msg.substr(0, pos);

        if (!param.empty())
        {
            bool match = true;

            if (param == "DANGER")
                channel = MSGCH_DANGER;
            else if (param == "WARN")
                channel = MSGCH_WARN;
            else if (param == "SOUND")
                channel = MSGCH_SOUND;
            else if (param == "PLAIN")
                channel = MSGCH_PLAIN;
            else if (param == "SPELL" || param == "ENCHANT")
                msg.clear(); // disallow these as well, channel stays TALK
            else if (param != "TALK")
                match = false;

            if (match && !msg.empty())
                msg = msg.substr(pos + 1);
        }
    }

    if (msg.empty()) // give default noises
    {
        channel = MSGCH_SOUND;
        msg = "You hear a strange noise.";
    }

    // replace weapon references
    if (weapon)
    {
        msg = replace_all(msg, "@The_weapon@", "The @weapon@");
        msg = replace_all(msg, "@the_weapon@", "the @weapon@");
        msg = replace_all(msg, "@weapon@", weapon->name(DESC_BASENAME));
    }
    // replace references to player name and god
    msg = replace_all(msg, "@player_name@", you.your_name);
    msg = replace_all(msg, "@player_god@",
                      you.religion == GOD_NO_GOD ? "atheism"
                      : god_name(you.religion, coinflip()));

    mpr(msg.c_str(), channel);

    noisy(25, you.pos());
}

void shadow_lantern_effect()
{
    if (x_chance_in_y(player_spec_death() + 1, 8))
    {
        create_monster(mgen_data(MONS_SHADOW, BEH_FRIENDLY, &you, 2, 0,
                                 you.pos(), MHITYOU));

        item_def *lantern = you.weapon();

        // This should only get called when we are wielding a lantern of
        // shadows.
        ASSERT(lantern && lantern->base_type == OBJ_MISCELLANY
                && lantern->sub_type == MISC_LANTERN_OF_SHADOWS);

        bool known = fully_identified(*lantern);
        did_god_conduct(DID_NECROMANCY, 1, known);

        // ID the lantern and refresh the weapon display.
        if (!known)
        {
            set_ident_type(*lantern, ID_KNOWN_TYPE);
            set_ident_flags(*lantern, ISFLAG_IDENT_MASK);

            you.wield_change = true;
        }
    }
}

void unrand_reacts()
{
    item_def*  weapon     = you.weapon();
    const int  old_plus   = weapon ? weapon->plus   : 0;
    const int  old_plus2  = weapon ? weapon->plus2  : 0;

    for (int i = 0; i < NUM_EQUIP; i++)
    {
        if (you.unrand_reacts & (1 << i))
        {
            item_def&        item  = you.inv[you.equip[i]];
            unrandart_entry* entry = get_unrand_entry(item.special);

            entry->world_reacts_func(&item);
        }
    }

    if (weapon && (old_plus != weapon->plus || old_plus2 != weapon->plus2))
        you.wield_change = true;
}

static bool _reaching_weapon_attack(const item_def& wpn)
{
    dist beam;

    direction_chooser_args args;
    args.restricts = DIR_TARGET;
    args.mode = TARG_HOSTILE;
    args.range = 2;
    args.top_prompt = "Attack whom?";

    direction(beam, args);

    if (!beam.isValid)
        return (false);

    if (beam.isMe())
    {
        canned_msg(MSG_UNTHINKING_ACT);
        return (false);
    }

    const coord_def delta = beam.target - you.pos();
    const int x_distance  = abs(delta.x);
    const int y_distance  = abs(delta.y);
    monsters* mons = monster_at(beam.target);

    const int x_middle = std::max(beam.target.x, you.pos().x)
                            - (x_distance / 2);
    const int y_middle = std::max(beam.target.y, you.pos().y)
                            - (y_distance / 2);
    const coord_def middle(x_middle, y_middle);

    if (x_distance > 2 || y_distance > 2)
    {
        mpr("Your weapon cannot reach that far!");
        return (false);
    }
    else if (!you.see_cell_no_trans(beam.target)
             && grd(middle) <= DNGN_MAX_NONREACH)
    {
        // Might also be a granite statue/orcish idol which you
        // can reach _past_.
        mpr("There's a wall in the way.");
        return (false);
    }
    else if (mons == NULL)
    {
        // Must return true, otherwise you get a free discovery
        // of invisible monsters.
        mpr("You attack empty space.");
        return (true);
    }

    // BCR - Added a check for monsters in the way.  Only checks cardinal
    //       directions.  Knight moves are ignored.  Assume the weapon
    //       slips between the squares.

    // If we're attacking more than a space away...
    if (x_distance > 1 || y_distance > 1)
    {
        bool success = false;
        // If either the x or the y is the same, we should check for
        // a monster:
        if ((beam.target.x == you.pos().x || beam.target.y == you.pos().y)
            && monster_at(middle))
        {
            const int skill = weapon_skill( wpn.base_type, wpn.sub_type );

            if (x_chance_in_y(5 + (3 * skill), 40))
            {
                mpr("You reach to attack!");
                success = you_attack(mons->mindex(), false);
            }
            else
            {
                mpr("You could not reach far enough!");
                return (true);
            }
        }
        else
        {
            mpr("You reach to attack!");
            success = you_attack(mons->mindex(), false);
        }

        if (success)
        {
            // Monster might have died or gone away.
            if (monsters* m = monster_at(beam.target))
                if (mons_is_mimic(m->type))
                    mimic_alert(m);
        }
    }
    else
        you_attack(mons->mindex(), false);

    return (true);
}

static bool _evoke_horn_of_geryon(item_def &item)
{
    // Note: This assumes that the Vestibule has not been changed.
    bool rc = false;

    if (player_in_branch( BRANCH_VESTIBULE_OF_HELL ))
    {
        mpr("You produce a weird and mournful sound.");

        for (int count_x = 0; count_x < GXM; count_x++)
            for (int count_y = 0; count_y < GYM; count_y++)
            {
                if (grd[count_x][count_y] == DNGN_STONE_ARCH)
                {
                    rc = true;

                    map_marker *marker =
                        env.markers.find(coord_def(count_x, count_y),
                                         MAT_FEATURE);

                    if (marker)
                    {
                        map_feature_marker *featm =
                            dynamic_cast<map_feature_marker*>(marker);
                        // [ds] Ensure we're activating the correct feature
                        // markers. Feature markers are also used for other
                        // things, notably to indicate the return point from
                        // a labyrinth or portal vault.
                        switch (featm->feat)
                        {
                        case DNGN_ENTER_COCYTUS:
                        case DNGN_ENTER_DIS:
                        case DNGN_ENTER_GEHENNA:
                        case DNGN_ENTER_TARTARUS:
                            grd[count_x][count_y] = featm->feat;
                            env.markers.remove(marker);
                            item.plus2++;
                            break;
                        default:
                            break;
                        }
                    }
                }
            }

        if (rc)
            mpr("Your way has been unbarred.");
    }
    else
    {
        mpr("You produce a hideous howling noise!", MSGCH_SOUND);
        create_monster(
            mgen_data::hostile_at(MONS_BEAST, "the horn of Geryon",
                true, 4, 0, you.pos()));
    }
    return (rc);
}

static bool _efreet_flask(int slot)
{
    bool friendly = x_chance_in_y(10 + you.skills[SK_EVOCATIONS] / 3, 20);

    mpr("You open the flask...");

    const int monster =
        create_monster(
            mgen_data(MONS_EFREET,
                      friendly ? BEH_FRIENDLY : BEH_HOSTILE,
                      &you, 0, 0, you.pos(),
                      MHITYOU, MG_FORCE_BEH));

    if (monster != -1)
    {
        mpr("...and a huge efreet comes out.");

        if (player_angers_monster(&menv[monster]))
            friendly = false;

        if (silenced(you.pos()))
        {
            mpr(friendly ? "It nods graciously at you."
                         : "It snaps in your direction!", MSGCH_TALK_VISUAL);
        }
        else
        {
            mpr(friendly ? "\"Thank you for releasing me!\""
                         : "It howls insanely!", MSGCH_TALK);
        }
    }
    else
        canned_msg(MSG_NOTHING_HAPPENS);

    dec_inv_item_quantity(slot, 1);

    return (true);
}

static bool _is_crystal_ball(const item_def &item)
{
    return (item.base_type == OBJ_MISCELLANY
            && (item.sub_type == MISC_CRYSTAL_BALL_OF_FIXATION
                || item.sub_type == MISC_CRYSTAL_BALL_OF_ENERGY
                || item.sub_type == MISC_CRYSTAL_BALL_OF_SEEING));
}

static bool _check_crystal_ball(int subtype, bool known)
{
    if (you.intel() <= 1)
    {
        mpr( "You lack the intelligence to focus on the shapes in the ball." );
        return (false);
    }

    if (you.confused())
    {
        mpr( "You are unable to concentrate on the shapes in the ball." );
        return (false);
    }

    if (subtype == MISC_CRYSTAL_BALL_OF_ENERGY && known
        && (you.magic_points == you.max_magic_points))
    {
        mpr( "With no energy to recover, the crystal ball of energy is "
             "presently useless to you.");
        return (false);
    }

    return (true);
}

static bool _ball_of_seeing(void)
{
    bool ret = false;

    mpr("You gaze into the crystal ball.");

    int use = random2(you.skills[SK_EVOCATIONS] * 6);

    if (use < 2)
    {
        lose_stat( STAT_INT, 1, false, "using a ball of seeing");
    }
    else if (use < 5 && enough_mp(1, true))
    {
        mpr("You feel power drain from you!");
        set_mp(0, false);
        // if you're out of mana, the switch chain falls through to confusion
    }
    else if (use < 10 || you.level_type == LEVEL_LABYRINTH)
    {
        if (you.level_type == LEVEL_LABYRINTH)
            mpr("You see a maze of twisty little passages, all alike.");
        confuse_player( 10 + random2(10), false );
    }
    else if (use < 15 || coinflip())
    {
        mpr("You see nothing.");
    }
    else if (magic_mapping( 15, 50 + random2( you.skills[SK_EVOCATIONS]), true))
    {
        mpr("You see a map of your surroundings!");
        ret = true;
    }
    else
    {
        mpr("You see nothing.");
    }

    return (ret);
}

static bool _disc_of_storms(void)
{
    const int fail_rate = (30 - you.skills[SK_EVOCATIONS]);
    bool rc = false;

    if (player_res_electricity() || x_chance_in_y(fail_rate, 100))
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (x_chance_in_y(fail_rate, 100))
        mpr("The disc glows for a moment, then fades.");
    else if (x_chance_in_y(fail_rate, 100))
        mpr("Little bolts of electricity crackle over the disc.");
    else
    {
        mpr("The disc erupts in an explosion of electricity!");
        rc = true;

        const int disc_count = roll_dice(2, 1 + you.skills[SK_EVOCATIONS] / 7);

        for (int i = 0; i < disc_count; ++i)
        {
            bolt beam;
            const zap_type types[] = { ZAP_LIGHTNING, ZAP_ELECTRICITY,
                                       ZAP_ORB_OF_ELECTRICITY };

            const zap_type which_zap = RANDOM_ELEMENT(types);

            beam.range = you.skills[SK_EVOCATIONS]/3 + 5; // 5--14
            beam.source = you.pos();
            beam.target = you.pos() + coord_def(random2(13)-6, random2(13)-6);

            // Non-controlleable, so no player tracer.
            zapping(which_zap, 30 + you.skills[SK_EVOCATIONS] * 2, beam);

        }

        for (radius_iterator ri(you.pos(), LOS_RADIUS, false); ri; ++ri)
        {
            if (grd(*ri) < DNGN_MAXWALL)
                continue;

            if (one_chance_in(60 - you.skills[SK_EVOCATIONS]))
                place_cloud(CLOUD_RAIN, *ri,
                            random2(you.skills[SK_EVOCATIONS]), KC_YOU);
        }
    }

    return (rc);
}

void tome_of_power(int slot)
{
    int powc = 5 + you.skills[SK_EVOCATIONS]
                 + roll_dice( 5, you.skills[SK_EVOCATIONS] );

    msg::stream << "The book opens to a page covered in "
                << weird_writing() << '.' << std::endl;

    you.turn_is_over = true;
    if (!item_ident(you.inv[slot], ISFLAG_KNOW_TYPE))
    {
        set_ident_flags(you.inv[slot], ISFLAG_KNOW_TYPE);

        if (!yesno("Read it?", false, 'n'))
            return;
    }

    if (player_mutation_level(MUT_BLURRY_VISION) > 0
        && x_chance_in_y(player_mutation_level(MUT_BLURRY_VISION), 4))
    {
        mpr("The page is too blurry for you to read.");
        return;
    }

    mpr("You find yourself reciting the magical words!");
    exercise( SK_EVOCATIONS, 1 );

    if (x_chance_in_y(7, 50))
    {
        mpr("A cloud of weird smoke pours from the book's pages!");
        big_cloud( random_smoke_type(), KC_YOU,
                   you.pos(), 20, 10 + random2(8) );
        xom_is_stimulated(16);
    }
    else if (x_chance_in_y(2, 43))
    {
        mpr("A cloud of choking fumes pours from the book's pages!");
        big_cloud(CLOUD_POISON, KC_YOU, you.pos(), 20, 7 + random2(5));
        xom_is_stimulated(64);
    }
    else if (x_chance_in_y(2, 41))
    {
        mpr("A cloud of freezing gas pours from the book's pages!");
        big_cloud(CLOUD_COLD, KC_YOU, you.pos(), 20, 8 + random2(5));
        xom_is_stimulated(64);
    }
    else if (x_chance_in_y(3, 39))
    {
        if (one_chance_in(5))
        {
            mpr("The book disappears in a mighty explosion!");
            dec_inv_item_quantity(slot, 1);
        }

        immolation(15, IMMOLATION_TOME, you.pos(), false, &you);

        xom_is_stimulated(255);
    }
    else if (one_chance_in(36))
    {
        if (create_monster(
                mgen_data::hostile_at(MONS_ABOMINATION_SMALL,
                    "a tome of Destruction",
                    true, 6, 0, you.pos())) != -1)
        {
            mpr("A horrible Thing appears!");
            mpr("It doesn't look too friendly.");
        }
        xom_is_stimulated(255);
    }
    else
    {
        viewwindow(false);

        int temp_rand = random2(23) + random2(you.skills[SK_EVOCATIONS] / 3);

        if (temp_rand > 25)
            temp_rand = 25;

        const spell_type spell_casted =
            ((temp_rand > 24) ? SPELL_LEHUDIBS_CRYSTAL_SPEAR :
             (temp_rand > 21) ? SPELL_BOLT_OF_FIRE :
             (temp_rand > 18) ? SPELL_BOLT_OF_COLD :
             (temp_rand > 16) ? SPELL_LIGHTNING_BOLT :
             (temp_rand > 10) ? SPELL_FIREBALL :
             (temp_rand >  9) ? SPELL_VENOM_BOLT :
             (temp_rand >  8) ? SPELL_BOLT_OF_DRAINING :
             (temp_rand >  7) ? SPELL_BOLT_OF_INACCURACY :
             (temp_rand >  6) ? SPELL_STICKY_FLAME :
             (temp_rand >  5) ? SPELL_TELEPORT_SELF :
             (temp_rand >  4) ? SPELL_CIGOTUVIS_DEGENERATION :
             (temp_rand >  3) ? SPELL_POLYMORPH_OTHER :
             (temp_rand >  2) ? SPELL_MEPHITIC_CLOUD :
             (temp_rand >  1) ? SPELL_THROW_FLAME :
             (temp_rand >  0) ? SPELL_THROW_FROST
                              : SPELL_MAGIC_DART);

        your_spells( spell_casted, powc, false );
    }
}

void skill_manual(int slot)
{
    // Removed confirmation request because you know it's
    // a manual in advance.
    you.turn_is_over = true;
    item_def& manual(you.inv[slot]);
    const bool known = item_type_known(manual);
    if (!known)
        set_ident_flags( manual, ISFLAG_KNOW_TYPE );
    const int skill = manual.plus;

    mprf("You read about %s.", skill_name(skill));

    exercise(skill, 500);

    if (--manual.plus2 <= 0)
    {
        mpr("The manual crumbles into dust.");
        dec_inv_item_quantity( slot, 1 );
    }
    else
        mpr("The manual looks somewhat more worn.");

    xom_is_stimulated(known ? 14 : 64);
}

static bool _box_of_beasts(item_def &box)
{
    bool success = false;

    mpr("You open the lid...");

    if (x_chance_in_y(60 + you.skills[SK_EVOCATIONS], 100))
    {
        monster_type beasty = MONS_NO_MONSTER;

        // If you worship a good god, don't summon an evil beast (in
        // this case, the hell hound).
        do
        {
            int temp_rand = random2(11);

            beasty = ((temp_rand == 0) ? MONS_GIANT_BAT :
                      (temp_rand == 1) ? MONS_HOUND :
                      (temp_rand == 2) ? MONS_JACKAL :
                      (temp_rand == 3) ? MONS_RAT :
                      (temp_rand == 4) ? MONS_ICE_BEAST :
                      (temp_rand == 5) ? MONS_SNAKE :
                      (temp_rand == 6) ? MONS_YAK :
                      (temp_rand == 7) ? MONS_BUTTERFLY :
                      (temp_rand == 8) ? MONS_WATER_MOCCASIN :
                      (temp_rand == 9) ? MONS_CROCODILE
                                       : MONS_HELL_HOUND);
        }
        while (player_will_anger_monster(beasty));

        beh_type beha = BEH_FRIENDLY;

        if (one_chance_in(you.skills[SK_EVOCATIONS] + 5))
            beha = BEH_HOSTILE;

        if (create_monster(
                mgen_data(beasty, beha, &you, 2 + random2(4), 0,
                          you.pos(), MHITYOU)) != -1)
        {
            success = true;

            mpr("...and something leaps out!");
            xom_is_stimulated(14);
        }
    }
    else
    {
        if (!one_chance_in(6))
            mpr("...but nothing happens.");
        else
        {
            mpr("...but the box appears empty.");
            box.sub_type = MISC_EMPTY_EBONY_CASKET;
        }
    }

    return (success);
}

static bool _ball_of_energy(void)
{
    bool ret = false;

    mpr("You gaze into the crystal ball.");

    int use = random2(you.skills[SK_EVOCATIONS] * 6);

    if (use < 2)
    {
        lose_stat(STAT_INT, 1, false, "using a ball of energy");
    }
    else if (use < 4 && enough_mp(1, true))
    {
        mpr( "You feel your power drain away!" );
        set_mp( 0, false );
    }
    else if (use < 6)
    {
        confuse_player( 10 + random2(10), false );
    }
    else
    {
        int proportional = (you.magic_points * 100) / you.max_magic_points;

        if (random2avg(77 - you.skills[SK_EVOCATIONS] * 2, 4) > proportional
            || one_chance_in(25))
        {
            mpr( "You feel your power drain away!" );
            set_mp( 0, false );
        }
        else
        {
            mpr( "You are suffused with power!" );
            inc_mp( 6 + roll_dice( 2, you.skills[SK_EVOCATIONS] ), false );

            ret = true;
        }
    }

    return (ret);
}

static bool _ball_of_fixation(void)
{
    mpr("You gaze into the crystal ball.");
    mpr("You are mesmerised by a rainbow of scintillating colours!");

    const int duration = random_range(15, 40);
    you.set_duration(DUR_PARALYSIS, duration);
    you.set_duration(DUR_SLOW,      duration);

    return (true);
}

bool evoke_item(int slot)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return (false);
    }

    if (!player_can_handle_equipment())
    {
        canned_msg(MSG_PRESENT_FORM);
        return (false);
    }

    if (slot == -1)
    {
        slot = prompt_invent_item( "Evoke which item? (* to show all)",
                                   MT_INVLIST,
                                   OSEL_EVOKABLE, true, true, true, 0, -1,
                                   NULL, OPER_EVOKE );

        if (prompt_failed(slot))
            return (false);
    }
    else if (!check_warning_inscriptions(you.inv[slot], OPER_EVOKE))
        return (false);

    ASSERT(slot >= 0);

#ifdef ASSERTS // Used only by an assert
    const bool wielded = (you.equip[EQ_WEAPON] == slot);
#endif /* DEBUG */

    item_def& item = you.inv[slot];
    // Also handles messages.
    if (!item_is_evokable(item, false, false, true))
        return (false);

    int pract = 0; // By how much Evocations is practised.
    bool did_work   = false;  // Used for default "nothing happens" message.
    bool unevokable = false;
    bool ident      = false;

    const unrandart_entry *entry = is_unrandom_artefact(item)
        ? get_unrand_entry(item.special) : NULL;

    if (entry && entry->evoke_func)
    {
        ASSERT(item_is_equipped(item));
        if (entry->evoke_func(&item, &pract, &did_work, &unevokable))
            return (did_work);
    }
    else switch (item.base_type)
    {
    case OBJ_WANDS:
        zap_wand(slot);
        return (true);

    case OBJ_WEAPONS:
        ASSERT(wielded);

        if (get_weapon_brand(item) == SPWPN_REACHING)
        {
            if (_reaching_weapon_attack(item))
            {
                pract    = 0;
                did_work = true;
            }
            else
                return (false);
        }
        else
            unevokable = true;
        break;

    case OBJ_STAVES:
        ASSERT(wielded);

        if (item_is_rod( item ))
        {
            pract = staff_spell( slot );
            // [ds] Early exit, no turns are lost.
            if (pract == -1)
                return (false);

            did_work = true;  // staff_spell() will handle messages
        }
        else if (item.sub_type == STAFF_CHANNELING)
        {
            if (you.magic_points < you.max_magic_points
                && x_chance_in_y(you.skills[SK_EVOCATIONS] + 11, 40))
            {
                mpr("You channel some magical energy.");
                inc_mp( 1 + random2(3), false );
                make_hungry(50, false, true);
                pract = 1;
                did_work = true;
                ident = true;
            }
        }
        else
        {
            unevokable = true;
        }
        break;

    case OBJ_MISCELLANY:
        did_work = true; // easier to do it this way for misc items

        if (is_deck(item))
        {
            ASSERT(wielded);
            evoke_deck(item);
            pract = 1;
            break;
        }

        if (_is_crystal_ball(item)
            && !_check_crystal_ball(item.sub_type, item_type_known(item)))
        {
            unevokable = true;
            break;
        }

        switch (item.sub_type)
        {
        case MISC_BOTTLED_EFREET:
            if (_efreet_flask(slot))
                pract = 2;
            break;

        case MISC_CRYSTAL_BALL_OF_SEEING:
            if (_ball_of_seeing())
                pract = 1, ident = true;
            break;

        case MISC_AIR_ELEMENTAL_FAN:
            if (you.skills[SK_EVOCATIONS] <= random2(30))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                cast_summon_elemental(100, GOD_NO_GOD, MONS_AIR_ELEMENTAL, 4);
                pract = (one_chance_in(5) ? 1 : 0);
                ident = true;
            }
            break;

        case MISC_LAMP_OF_FIRE:
            if (you.skills[SK_EVOCATIONS] <= random2(30))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                cast_summon_elemental(100, GOD_NO_GOD, MONS_FIRE_ELEMENTAL, 4);
                pract = (one_chance_in(5) ? 1 : 0);
                ident = true;
            }
            break;

        case MISC_STONE_OF_EARTH_ELEMENTALS:
            if (you.skills[SK_EVOCATIONS] <= random2(30))
                canned_msg(MSG_NOTHING_HAPPENS);
            else
            {
                cast_summon_elemental(100, GOD_NO_GOD, MONS_EARTH_ELEMENTAL, 4, 5);
                pract = (one_chance_in(5) ? 1 : 0);
                ident = true;
            }
            break;

        case MISC_HORN_OF_GERYON:
            if (_evoke_horn_of_geryon(item))
                pract = 1;
            break;

        case MISC_BOX_OF_BEASTS:
            if (_box_of_beasts(item))
                pract = 1;
            break;

        case MISC_CRYSTAL_BALL_OF_ENERGY:
            if (_ball_of_energy())
                pract = 1, ident = true;
            break;

        case MISC_CRYSTAL_BALL_OF_FIXATION:
            if (_ball_of_fixation())
                pract = 1, ident = true;
            break;

        case MISC_DISC_OF_STORMS:
            if (_disc_of_storms())
                pract = (coinflip() ? 2 : 1), ident = true;
            break;

        default:
            did_work = false;
            unevokable = true;
            break;
        }
        break;

    default:
        unevokable = true;
        break;
    }

    if (!did_work)
        canned_msg(MSG_NOTHING_HAPPENS);
    else if (pract > 0)
        exercise( SK_EVOCATIONS, pract );

    if (ident && !item_type_known(item))
    {
        set_ident_type( item.base_type, item.sub_type, ID_KNOWN_TYPE );
        set_ident_flags( item, ISFLAG_KNOW_TYPE );

        mprf("You are wielding %s.",
             item.name(DESC_NOCAP_A).c_str());

        you.wield_change = true;
    }

    if (!unevokable)
        you.turn_is_over = true;
    else
        crawl_state.zero_turns_taken();

    return (did_work);
}
