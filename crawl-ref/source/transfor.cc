/*
 *  File:       transfor.cc
 *  Summary:    Misc function related to player transformations.
 *  Written by: Linley Henzell
 *
 *  Modified for Crawl Reference by $Author$ on $Date$
 *
 *  Change History (most recent first):
 *
 * <2>  5/26/99  JDJ  transform() and untransform() set you.wield_change so
 *                    the weapon line updates.
 * <1> -/--/--   LRH  Created
 */

#include "AppHdr.h"
#include "transfor.h"

#include <stdio.h>
#include <string.h>

#include "externs.h"

#include "delay.h"
#include "it_use2.h"
#include "itemprop.h"
#include "items.h"
#include "misc.h"
#include "player.h"
#include "skills2.h"
#include "stuff.h"

void drop_everything(void);
void extra_hp(int amount_extra);

bool remove_equipment(FixedVector < char, 8 > &remove_stuff)
{
    // if we're removing body armour, the cloak will come off as well -- bwr
    if (remove_stuff[EQ_BODY_ARMOUR] == 1 && you.equip[EQ_BODY_ARMOUR] != -1)
        remove_stuff[EQ_CLOAK] = 1;

    // if we're removing gloves, the weapon will come off as well -- bwr
    if (remove_stuff[EQ_GLOVES] == 1 && you.equip[EQ_GLOVES] != -1)
        remove_stuff[EQ_WEAPON] = 1;

    if (remove_stuff[EQ_WEAPON] == 1 && you.equip[EQ_WEAPON] != -1)
    {
        unwield_item(you.equip[EQ_WEAPON]);
        you.equip[EQ_WEAPON] = -1;
        mpr("You are empty-handed.");
        you.wield_change = true;
    }

    for (int i = EQ_CLOAK; i < EQ_LEFT_RING; i++)
    {
        if (remove_stuff[i] == 0 || you.equip[i] == -1)
            continue;

        mprf("%s falls away.",
             you.inv[you.equip[i]].name(DESC_CAP_YOUR).c_str());

        unwear_armour( you.equip[i] );
        you.equip[i] = -1;
    }

    return true;
}                               // end remove_equipment()

// Returns true if any piece of equipment that has to be removed is cursed.
// Useful for keeping low level transformations from being too useful.
static bool check_for_cursed_equipment( FixedVector < char, 8 > &remove_stuff )
{
    for (int i = EQ_WEAPON; i < EQ_LEFT_RING; i++)
    {
        if (remove_stuff[i] == 0 || you.equip[i] == -1)
            continue;

        if (item_cursed( you.inv[ you.equip[i] ] ))
        {
            mpr( "Your cursed equipment won't allow you to complete the "
                 "transformation." );

            return (true);
        }
    }

    return (false);
}                               // end check_for_cursed_equipment()

// FIXME: Switch to 4.1 transforms handling.
size_type transform_size(int psize)
{
    return you.transform_size(psize);
}

size_type player::transform_size(int psize) const
{
    const int transform = attribute[ATTR_TRANSFORMATION];
    switch (transform)
    {
    case TRAN_SPIDER:
        return SIZE_TINY;
    case TRAN_ICE_BEAST:
        return SIZE_LARGE;
    case TRAN_DRAGON:
    case TRAN_SERPENT_OF_HELL:
        return SIZE_HUGE;
    case TRAN_AIR:
        return SIZE_MEDIUM;
    default:
        return SIZE_CHARACTER;
    }
}

bool transform(int pow, transformation_type which_trans)
{
    if (you.species == SP_MERFOLK && player_is_swimming()
        && which_trans != TRAN_DRAGON)
    {
        // This might by overkill, but it's okay because obviously
        // whatever magical ability that let's them walk on land is
        // removed when they're in water (in this case, their natural
        // form is completely over-riding any other... goes well with
        // the forced transform when entering water)... but merfolk can
        // transform into dragons, because dragons fly. -- bwr
        mpr("You cannot transform out of your normal form while in water.");
        return (false);
    }

    // This must occur before the untransform() and the is_undead check.
    if (you.attribute[ATTR_TRANSFORMATION] == which_trans)
    {
        if (you.duration[DUR_TRANSFORMATION] < 100)
        {
            mpr( "You extend your transformation's duration." );
            you.duration[DUR_TRANSFORMATION] += random2(pow);

            if (you.duration[DUR_TRANSFORMATION] > 100)
                you.duration[DUR_TRANSFORMATION] = 100;

            return (true);
        }
        else
        {
            mpr( "You cannot extend your transformation any further!" );
            return (false);
        }
    }

    if (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE)
        untransform();

    if (you.is_undead)
    {
        mpr("Your unliving flesh cannot be transformed in this way.");
        return (false);
    }

    //jmf: silently discard this enchantment
    you.duration[DUR_STONESKIN] = 0;

    FixedVector < char, 8 > rem_stuff;

    for (int i = EQ_WEAPON; i < EQ_RIGHT_RING; i++)
        rem_stuff[i] = 1;

    you.redraw_evasion = 1;
    you.redraw_armour_class = 1;
    you.wield_change = true;

    /* Remember, it can still fail in the switch below... */
    switch (which_trans)
    {
    case TRAN_SPIDER:           // also AC +3, ev +3, fast_run
        if (check_for_cursed_equipment( rem_stuff ))
            return (false);

        mpr("You turn into a venomous arachnid creature.");
        remove_equipment( rem_stuff );

        you.attribute[ATTR_TRANSFORMATION] = TRAN_SPIDER;
        you.duration[DUR_TRANSFORMATION] = 10 + random2(pow) + random2(pow);

        if (you.duration[DUR_TRANSFORMATION] > 60)
            you.duration[DUR_TRANSFORMATION] = 60;

        modify_stat( STAT_DEXTERITY, 5, true );

        you.symbol = 's';
        you.colour = BROWN;
        return (true);

    case TRAN_ICE_BEAST:  // also AC +3, cold +3, fire -1, pois +1 
        mpr( "You turn into a creature of crystalline ice." );

        rem_stuff[ EQ_CLOAK ] = 0;       

        remove_equipment( rem_stuff );

        you.attribute[ATTR_TRANSFORMATION] = TRAN_ICE_BEAST;
        you.duration[DUR_TRANSFORMATION] = 30 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        extra_hp(12);   // must occur after attribute set

        if (you.duration[DUR_ICY_ARMOUR])
            mpr( "Your new body merges with your icy armour." );

        you.symbol = 'I';
        you.colour = WHITE;
        return (true);

    case TRAN_BLADE_HANDS:
        rem_stuff[EQ_CLOAK] = 0;
        rem_stuff[EQ_HELMET] = 0;
        rem_stuff[EQ_BOOTS] = 0;
        rem_stuff[EQ_BODY_ARMOUR] = 0;

        if (check_for_cursed_equipment( rem_stuff ))
            return (false);

        mpr("Your hands turn into razor-sharp scythe blades.");
        remove_equipment( rem_stuff );

        you.attribute[ATTR_TRANSFORMATION] = TRAN_BLADE_HANDS;
        you.duration[DUR_TRANSFORMATION] = 10 + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;
        return (true);

    case TRAN_STATUE: // also AC +20, ev -5, elec +1, pois +1, neg +1, slow
        if (you.species == SP_GNOME && coinflip())
            mpr( "Look, a garden gnome.  How cute!" );
        else if (player_genus(GENPC_DWARVEN) && one_chance_in(10))
            mpr( "You inwardly fear your resemblance to a lawn ornament." );
        else
            mpr( "You turn into a living statue of rough stone." );

        rem_stuff[ EQ_WEAPON ] = 0;       /* can still hold a weapon */
        rem_stuff[ EQ_CLOAK ] = 0;       
        rem_stuff[ EQ_HELMET ] = 0;       
        rem_stuff[ EQ_BOOTS ] = 0;       
        // too stiff to make use of shields, gloves, or armour -- bwr

        remove_equipment( rem_stuff );

        you.attribute[ATTR_TRANSFORMATION] = TRAN_STATUE;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        modify_stat( STAT_DEXTERITY, -2, true );
        modify_stat( STAT_STRENGTH, 2, true );
        extra_hp(15);   // must occur after attribute set

        if (you.duration[DUR_STONEMAIL] || you.duration[DUR_STONESKIN])
            mpr( "Your new body merges with your stone armour." );

        you.symbol = '8';
        you.colour = LIGHTGREY;
        return (true);

    case TRAN_DRAGON:  // also AC +10, ev -3, cold -1, fire +2, pois +1, flight
        if (you.species == SP_MERFOLK && player_is_swimming())
            mpr("You fly out of the water as you turn into a fearsome dragon!");
        else
            mpr("You turn into a fearsome dragon!");

        remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_DRAGON;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        modify_stat( STAT_STRENGTH, 10, true );
        extra_hp(16);   // must occur after attribute set

        you.symbol = 'D';
        you.colour = GREEN;
        return (true);

    case TRAN_LICH:
        // also AC +3, cold +1, neg +3, pois +1, is_undead, res magic +50,
        // spec_death +1, and drain attack (if empty-handed)
        if (you.duration[DUR_DEATHS_DOOR])
        {
            mpr( "The transformation conflicts with an enchantment "
                 "already in effect." );

            return (false);
        }

        mpr("Your body is suffused with negative energy!");

        // undead cannot regenerate -- bwr
        if (you.duration[DUR_REGENERATION])
        {
            mpr( "You stop regenerating.", MSGCH_DURATION );
            you.duration[DUR_REGENERATION] = 0;
        }

        // silently removed since undead automatically resist poison -- bwr
        you.duration[DUR_RESIST_POISON] = 0;

        /* no remove_equip */
        you.attribute[ATTR_TRANSFORMATION] = TRAN_LICH;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 100)
            you.duration[ DUR_TRANSFORMATION ] = 100;

        modify_stat( STAT_STRENGTH, 3, true );
        you.symbol = 'L';
        you.colour = LIGHTGREY;
        you.is_undead = US_UNDEAD;
        you.hunger_state = HS_SATIATED;  // no hunger effects while transformed
        set_redraw_status( REDRAW_HUNGER );
        return (true);

    case TRAN_AIR:
        // also AC 20, ev +20, regen/2, no hunger, fire -2, cold -2, air +2, 
        // pois +1, spec_earth -1
        mpr( "You feel diffuse..." );

        remove_equipment(rem_stuff);

        drop_everything();

        you.attribute[ATTR_TRANSFORMATION] = TRAN_AIR;
        you.duration[DUR_TRANSFORMATION] = 35 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 150)
            you.duration[ DUR_TRANSFORMATION ] = 150;

        modify_stat( STAT_DEXTERITY, 8, true );
        you.symbol = '#';
        you.colour = DARKGREY;
        return (true);

    case TRAN_SERPENT_OF_HELL:
        // also AC +10, ev -5, fire +2, pois +1, life +2, slow
        mpr( "You transform into a huge demonic serpent!" );

        remove_equipment(rem_stuff);

        you.attribute[ATTR_TRANSFORMATION] = TRAN_SERPENT_OF_HELL;
        you.duration[DUR_TRANSFORMATION] = 20 + random2(pow) + random2(pow);

        if (you.duration[ DUR_TRANSFORMATION ] > 120)
            you.duration[ DUR_TRANSFORMATION ] = 120;

        modify_stat( STAT_STRENGTH, 13, true );
        extra_hp(17);   // must occur after attribute set

        you.symbol = 'S';
        you.colour = RED;
        return (true);
    case TRAN_NONE:
    case NUM_TRANSFORMATIONS:
        break;
    }

    return (false);
}                               // end transform()

bool transform_can_butcher_barehanded(transformation_type tt)
{
    return (tt == TRAN_BLADE_HANDS || tt == TRAN_DRAGON);
}

void untransform(void)
{
    FixedVector < char, 8 > rem_stuff;

    for (int i = EQ_WEAPON; i < EQ_RIGHT_RING; i++)
        rem_stuff[i] = 0;

    you.redraw_evasion = 1;
    you.redraw_armour_class = 1;
    you.wield_change = true;

    you.symbol = '@';
    you.colour = LIGHTGREY;

    // must be unset first or else infinite loops might result -- bwr
    const transformation_type old_form =
        static_cast<transformation_type>(you.attribute[ ATTR_TRANSFORMATION ]);
    
    you.attribute[ ATTR_TRANSFORMATION ] = TRAN_NONE;
    you.duration[ DUR_TRANSFORMATION ] = 0;

    int hp_downscale = 10;
    
    switch (old_form)
    {
    case TRAN_SPIDER:
        mpr("Your transformation has ended.", MSGCH_DURATION);
        modify_stat( STAT_DEXTERITY, -5, true );
        break;

    case TRAN_BLADE_HANDS:
        mpr( "Your hands revert to their normal proportions.", MSGCH_DURATION );
        you.wield_change = true;
        break;

    case TRAN_STATUE:
        mpr( "You revert to your normal fleshy form.", MSGCH_DURATION );
        modify_stat( STAT_DEXTERITY, 2, true );
        modify_stat( STAT_STRENGTH, -2, true );

        // Note: if the core goes down, the combined effect soon disappears, 
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_STONEMAIL])
            you.duration[DUR_STONEMAIL] = 1;

        if (you.duration[DUR_STONESKIN])
            you.duration[DUR_STONESKIN] = 1;

        hp_downscale = 15;
        
        break;

    case TRAN_ICE_BEAST:
        mpr( "You warm up again.", MSGCH_DURATION );

        // Note: if the core goes down, the combined effect soon disappears, 
        // but the reverse isn't true. -- bwr
        if (you.duration[DUR_ICY_ARMOUR])
            you.duration[DUR_ICY_ARMOUR] = 1;

        hp_downscale = 12;
        
        break;

    case TRAN_DRAGON:
        mpr( "Your transformation has ended.", MSGCH_DURATION );
        modify_stat(STAT_STRENGTH, -10, true);

        // re-check terrain now that be may no longer be flying.
        move_player_to_grid( you.x_pos, you.y_pos, false, true, true );

        hp_downscale = 16;
        
        break;

    case TRAN_LICH:
        mpr( "You feel yourself come back to life.", MSGCH_DURATION );
        modify_stat(STAT_STRENGTH, -3, true);
        you.is_undead = US_ALIVE;
        break;

    case TRAN_AIR:
        mpr( "Your body solidifies.", MSGCH_DURATION );
        modify_stat(STAT_DEXTERITY, -8, true);
        break;

    case TRAN_SERPENT_OF_HELL:
        mpr( "Your transformation has ended.", MSGCH_DURATION );
        modify_stat(STAT_STRENGTH, -13, true);
        hp_downscale = 17;
        break;

    default:
        break;
    }

    if (transform_can_butcher_barehanded(old_form))
        stop_butcher_delay();
    
    // If nagas wear boots while transformed, they fall off again afterwards:
    // I don't believe this is currently possible, and if it is we
    // probably need something better to cover all possibilities.  -bwr
    if ((you.species == SP_NAGA || you.species == SP_CENTAUR)
            && you.equip[ EQ_BOOTS ] != -1
            && you.inv[ you.equip[EQ_BOOTS] ].sub_type != ARM_NAGA_BARDING)
    {
        rem_stuff[EQ_BOOTS] = 1;
        remove_equipment(rem_stuff);
    }

    calc_hp();
    if (hp_downscale != 10)
    {
        you.hp = you.hp * 10 / hp_downscale;
        if (you.hp < 1)
            you.hp = 1;
        else if (you.hp > you.hp_max)
            you.hp = you.hp_max;
    }
}                               // end untransform()

// XXX: This whole system is a mess as it still relies on special
// cases to handle a large number of things (see wear_armour()) -- bwr
bool can_equip( equipment_type use_which )
{

    // if more cases are added to this if must also change in
    // item_use for naga barding
    if (!player_is_shapechanged())
        /* or a transformation which doesn't change overall shape */
    {
        if (use_which == EQ_BOOTS)
        {
            switch (you.species)
            {
            case SP_NAGA:
            case SP_CENTAUR:
            case SP_KENKU:
                return (false);
            default:
                break;
            }
        }
        else if (use_which == EQ_HELMET)
        {
            switch (you.species)
            {
            case SP_KENKU:
                return (false);
            default:
                break;
            }
        }
    }

    if (use_which == EQ_HELMET && you.mutation[MUT_HORNS])
        return (false);

    if (use_which == EQ_BOOTS && you.mutation[MUT_HOOVES])
        return (false);

    if (use_which == EQ_GLOVES && you.mutation[MUT_CLAWS] >= 3)
        return (false);

    switch (you.attribute[ATTR_TRANSFORMATION])
    {
    case TRAN_NONE:
    case TRAN_LICH:
        return (true);

    case TRAN_BLADE_HANDS:
        return (use_which != EQ_WEAPON
                && use_which != EQ_GLOVES
                && use_which != EQ_SHIELD);

    case TRAN_STATUE:
        return (use_which == EQ_WEAPON 
                || use_which == EQ_CLOAK 
                || use_which == EQ_HELMET);

    case TRAN_ICE_BEAST:
        return (use_which == EQ_CLOAK);

    default:
        return (false);
    }

    return (true);
}                               // end can_equip()

// raw comparison of an item, must use check_armour_shape for full version 
bool transform_can_equip_type( int eq_slot )
{
    // FIXME FIXME FIXME
    return (false);

    // const int form = you.attribute[ATTR_TRANSFORMATION];
    // return (!must_remove( Trans[form].rem_stuff, eq_slot ));
}

void extra_hp(int amount_extra) // must also set in calc_hp
{
    calc_hp();

    you.hp *= amount_extra;
    you.hp /= 10;

    deflate_hp(you.hp_max, false);
}                               // end extra_hp()

void drop_everything(void)
{
    int i = 0;

    if (inv_count() < 1)
        return;

    mpr( "You find yourself unable to carry your possessions!" );

    for (i = 0; i < ENDOFPACK; i++)
    {
        if (is_valid_item( you.inv[i] ))
        {
            copy_item_to_grid( you.inv[i], you.x_pos, you.y_pos );
            you.inv[i].quantity = 0;
        }
    }

    return;
}                               // end drop_everything()

// Used to mark transformations which override species/mutation intrinsics.
// If phys_scales is true then we're checking to see if the form keeps 
// the physical (AC/EV) properties from scales... the special intrinsic 
// features (resistances, etc) are lost in those forms however.
bool transform_changed_physiology( bool phys_scales )
{
    return (you.attribute[ATTR_TRANSFORMATION] != TRAN_NONE
            && you.attribute[ATTR_TRANSFORMATION] != TRAN_BLADE_HANDS
            && (!phys_scales 
                || (you.attribute[ATTR_TRANSFORMATION] != TRAN_LICH
                    && you.attribute[ATTR_TRANSFORMATION] != TRAN_STATUE)));
}
