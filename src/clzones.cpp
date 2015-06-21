#include "clzones.h"
#include "map.h"
#include "game.h"
#include "debug.h"
#include "output.h"
#include "mapsharing.h"
#include "translations.h"
#include "worldfactory.h"
#include "catacharset.h"
#include "ui.h"
#include <iostream>
#include <fstream>

zone_manager::zone_manager()
{
    //Add new zone types here
    //Use: if (g->checkZone("ZONE_NAME", posx, posy)) {
    //to check for a zone

    types.push_back( std::make_pair( _("No Auto Pickup"), "NO_AUTO_PICKUP" ) );
    types.push_back( std::make_pair( _("No NPC Pickup"),  "NO_NPC_PICKUP" ) );
}

void zone_manager::zone_data::set_name()
{
    const std::string name = string_input_popup( _( "Zone name:" ), 55, this->name, "", "", 15 );

    this->name = ( name == "" ) ? _( "<no name>" ) : name;
}

void zone_manager::zone_data::set_zone_type(
    const std::vector<std::pair<std::string, std::string> > &types )
{
    uimenu as_m;
    as_m.text = _( "Select zone type:" );

    for( unsigned int i = 0; i < types.size(); ++i ) {
        as_m.entries.push_back( uimenu_entry( i + 1, true, ( char )i + 1, types[i].first ) );
    }
    as_m.query();

    this->type = types[( ( as_m.ret >= 1 ) ? as_m.ret : 1 ) - 1].second;
}

void zone_manager::zone_data::set_enabled( const bool _enabled )
{
    enabled = _enabled;
}

tripoint zone_manager::zone_data::get_center_point() const
{
    return tripoint( ( start.x + end.x ) / 2, ( start.y + end.y ) / 2, ( start.z + end.z ) / 2 );
}

std::string zone_manager::get_name_from_type( const std::string &type ) const
{
    for( auto &elem : types ) {
        if( elem.second == type ) {
            return elem.first;
        }
    }

    return _("Unknown Type");
}

bool zone_manager::has_type( const std::string &type ) const
{
    for( auto &elem : types ) {
        if( elem.second == type ) {
            return true;
        }
    }

    return false;
}

void zone_manager::cache_zone_data()
{
    area_cache.clear();

    for( auto &elem : zones ) {
        if( elem.get_enabled() ) {
            const std::string sType = elem.get_zone_type();

            tripoint start = elem.get_start_point();
            tripoint end = elem.get_end_point();

            //draw marked area
            for( int y = start.y; y <= end.y; ++y ) {
                for( int x = start.x; x <= end.x; ++x ) {
                    for( int z = start.z; z <= end.z; ++z ) {
                        area_cache[sType].insert( tripoint{ x, y, z } );
                    }
                }
            }
        }
    }
}

bool zone_manager::has_zone( const std::string &type, const tripoint &where ) const
{
    const auto type_iter = area_cache.find( type );
    if( type_iter == area_cache.end() ) {
        return false;
    }

    const auto &point_set = type_iter->second;
    return point_set.find( where ) != point_set.end();
}

void zone_manager::serialize( JsonOut &json ) const
{
    json.start_array();
    for( auto &elem : zones ) {
        json.start_object();

        json.member( "name", elem.get_name() );
        json.member( "type", elem.get_zone_type() );
        json.member( "invert", elem.get_invert() );
        json.member( "enabled", elem.get_enabled() );

        tripoint start = elem.get_start_point();
        tripoint end = elem.get_end_point();

        json.member( "start_x", start.x );
        json.member( "start_y", start.y );
        json.member( "start_z", start.z );
        json.member( "end_x", end.x );
        json.member( "end_y", end.y );
        json.member( "end_z", end.z );

        json.end_object();
    }

    json.end_array();
}

void zone_manager::deserialize( JsonIn &jsin )
{
    zones.clear();

    jsin.start_array();
    while( !jsin.end_array() ) {
        JsonObject jo_zone = jsin.get_object();

        const std::string name = jo_zone.get_string( "name" );
        const std::string type = jo_zone.get_string( "type" );

        const bool invert = jo_zone.get_bool( "invert" );
        const bool enabled = jo_zone.get_bool( "enabled" );

        // Z coords need to have a default value - old saves won't have those
        const int start_x = jo_zone.get_int( "start_x" );
        const int start_y = jo_zone.get_int( "start_y" );
        const int start_z = jo_zone.get_int( "start_z", 0 );
        const int end_x = jo_zone.get_int( "end_x" );
        const int end_y = jo_zone.get_int( "end_y" );
        const int end_z = jo_zone.get_int( "end_z", 0 );

        if( has_type( type ) ) {
            add( name, type, invert, enabled,
                 tripoint( start_x, start_y, start_z ),
                 tripoint( end_x, end_y, end_z ) );
        }
    }

    cache_zone_data();
}


bool zone_manager::save_zones()
{
    cache_zone_data();

    std::string savefile = world_generator->active_world->world_path + "/" + base64_encode(
                               g->u.name ) + ".zones.json";

    try {
        std::ofstream fout;
        fout.exceptions( std::ios::badbit | std::ios::failbit );

        fopen_exclusive( fout, savefile.c_str() );
        if( !fout.is_open() ) {
            return true; //trick game into thinking it was saved
        }

        fout << serialize();
        fclose_exclusive( fout, savefile.c_str() );
        return true;

    } catch( std::ios::failure & ) {
        popup( _( "Failed to save zones to %s" ), savefile.c_str() );
        return false;
    }
}

void zone_manager::load_zones()
{
    std::string savefile = world_generator->active_world->world_path + "/" + base64_encode(
                               g->u.name ) + ".zones.json";

    std::ifstream fin;
    fin.open( savefile.c_str(), std::ifstream::in | std::ifstream::binary );
    if( !fin.good() ) {
        fin.close();
        return;
    }

    try {
        JsonIn jsin( fin );
        deserialize( jsin );
    } catch( std::string e ) {
        DebugLog( D_ERROR, DC_ALL ) << "load_zones: " << e;
    }

    fin.close();
}
