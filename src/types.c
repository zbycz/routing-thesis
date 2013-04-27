/***************************************
 Functions for handling the data types.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2008-2013 Andrew M. Bishop

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***************************************/


#include <string.h>

#include "types.h"


/*++++++++++++++++++++++++++++++++++++++
  Decide on the type of a way given the "highway" parameter.

  Highway HighwayType Returns the highway type of the way.

  const char *highway The string containing the type of the way.
  ++++++++++++++++++++++++++++++++++++++*/

Highway HighwayType(const char *highway)
{
 switch(*highway)
   {
   case 'c':
    if(!strcmp(highway,"cycleway")) return(Highway_Cycleway);
    break;

   case 'f':
    if(!strcmp(highway,"ferry")) return(Highway_Ferry);
    break;

   case 'm':
    if(!strcmp(highway,"motorway")) return(Highway_Motorway);
    break;

   case 'p':
    if(!strcmp(highway,"primary")) return(Highway_Primary);
    if(!strcmp(highway,"path")) return(Highway_Path);
    break;

   case 'r':
    if(!strcmp(highway,"residential")) return(Highway_Residential);
    break;

   case 's':
    if(!strcmp(highway,"secondary")) return(Highway_Secondary);
    if(!strcmp(highway,"service")) return(Highway_Service);
    if(!strcmp(highway,"steps")) return(Highway_Steps);
    break;

   case 't':
    if(!strcmp(highway,"trunk")) return(Highway_Trunk);
    if(!strcmp(highway,"tertiary")) return(Highway_Tertiary);
    if(!strcmp(highway,"track")) return(Highway_Track);
    break;

   case 'u':
    if(!strcmp(highway,"unclassified")) return(Highway_Unclassified);
    break;

   default:
    ;
   }

 return(Highway_None);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide on the type of transport given the name of it.

  Transport TransportType Returns the type of the transport.

  const char *transport The string containing the method of transport.
  ++++++++++++++++++++++++++++++++++++++*/

Transport TransportType(const char *transport)
{
 switch(*transport)
   {
   case 'b':
    if(!strcmp(transport,"bicycle"))
       return(Transport_Bicycle);
    break;

   case 'f':
    if(!strcmp(transport,"foot"))
       return(Transport_Foot);
    break;

   case 'g':
    if(!strcmp(transport,"goods"))
       return(Transport_Goods);
    break;

   case 'h':
    if(!strcmp(transport,"horse"))
       return(Transport_Horse);
    if(!strcmp(transport,"hgv"))
       return(Transport_HGV);
    break;

   case 'm':
    if(!strcmp(transport,"moped"))
       return(Transport_Moped);
    if(!strcmp(transport,"motorcycle"))
       return(Transport_Motorcycle);
    if(!strcmp(transport,"motorcar"))
       return(Transport_Motorcar);
    break;

   case 'p':
    if(!strcmp(transport,"psv"))
       return(Transport_PSV);
    break;

   case 'w':
    if(!strcmp(transport,"wheelchair"))
       return(Transport_Wheelchair);
    break;

   default:
    return(Transport_None);
   }

 return(Transport_None);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide on the type of property given the name of it.

  Property PropertyType Returns the type of the property.

  const char *property The string containing the method of property.
  ++++++++++++++++++++++++++++++++++++++*/

Property PropertyType(const char *property)
{
 switch(*property)
   {
   case 'b':
    if(!strcmp(property,"bicycleroute"))
       return(Property_BicycleRoute);

    if(!strcmp(property,"bridge"))
       return(Property_Bridge);
    break;

   case 'f':
    if(!strcmp(property,"footroute"))
       return(Property_FootRoute);
    break;

   case 'm':
    if(!strcmp(property,"multilane"))
       return(Property_Multilane);
    break;

   case 'p':
    if(!strcmp(property,"paved"))
       return(Property_Paved);
    break;

   case 't':
    if(!strcmp(property,"tunnel"))
       return(Property_Tunnel);
    break;

   default:
    return(Property_None);
   }

 return(Property_None);
}


/*++++++++++++++++++++++++++++++++++++++
  A string containing the name of a type of highway.

  const char *HighwayName Returns the name.

  Highway highway The highway type.
  ++++++++++++++++++++++++++++++++++++++*/

const char *HighwayName(Highway highway)
{
 switch(highway)
   {
   case Highway_None:
    return("NONE");

   case Highway_Motorway:
    return("motorway");
   case Highway_Trunk:
    return("trunk");
   case Highway_Primary:
    return("primary");
   case Highway_Secondary:
    return("secondary");
   case Highway_Tertiary:
    return("tertiary");
   case Highway_Unclassified:
    return("unclassified");
   case Highway_Residential:
    return("residential");
   case Highway_Service:
    return("service");
   case Highway_Track:
    return("track");
   case Highway_Cycleway:
    return("cycleway");
   case Highway_Path:
    return("path");
   case Highway_Steps:
    return("steps");
   case Highway_Ferry:
    return("ferry");

   case Highway_Count:
    ;

   case Highway_OneWay:
    ;
   case Highway_Roundabout:
    ;
   }

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  A string containing the name of a type of transport.

  const char *TransportName Returns the name.

  Transport transport The transport type.
  ++++++++++++++++++++++++++++++++++++++*/

const char *TransportName(Transport transport)
{
 switch(transport)
   {
   case Transport_None:
    return("NONE");

   case Transport_Foot:
    return("foot");
   case Transport_Horse:
    return("horse");
   case Transport_Wheelchair:
    return("wheelchair");
   case Transport_Bicycle:
    return("bicycle");
   case Transport_Moped:
    return("moped");
   case Transport_Motorcycle:
    return("motorcycle");
   case Transport_Motorcar:
    return("motorcar");
   case Transport_Goods:
    return("goods");
   case Transport_HGV:
    return("hgv");
   case Transport_PSV:
    return("psv");

   case Transport_Count:
    ;
  }

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  A string containing the name of a highway property.

  const char *PropertyName Returns the name.

  Property property The property type.
  ++++++++++++++++++++++++++++++++++++++*/

const char *PropertyName(Property property)
{
 switch(property)
   {
   case Property_None:
    return("NONE");

   case Property_Paved:
    return("paved");

   case Property_Multilane:
    return("multilane");

   case Property_Bridge:
    return("bridge");

   case Property_Tunnel:
    return("tunnel");

   case Property_FootRoute:
    return("footroute");

   case Property_BicycleRoute:
    return("bicycleroute");

   case Property_Count:
    ;
  }

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  A string containing the names of highways.

  const char *HighwaysNameList Returns the list of names.

  highways_t highways The highways type.
  ++++++++++++++++++++++++++++++++++++++*/

const char *HighwaysNameList(highways_t highways)
{
 static char string[256];

 string[0]=0;

 if(highways & Highways_Motorway)
    strcat(string,"motorway");

 if(highways & Highways_Trunk)
   {
    if(*string) strcat(string,", ");
    strcat(string,"trunk");
   }

 if(highways & Highways_Primary)
   {
    if(*string) strcat(string,", ");
    strcat(string,"primary");
   }

 if(highways & Highways_Tertiary)
   {
    if(*string) strcat(string,", ");
    strcat(string,"tertiary");
   }

 if(highways & Highways_Unclassified)
   {
    if(*string) strcat(string,", ");
    strcat(string,"unclassified");
   }

 if(highways & Highways_Residential)
   {
    if(*string) strcat(string,", ");
    strcat(string,"residential");
   }

 if(highways & Highways_Service)
   {
    if(*string) strcat(string,", ");
    strcat(string,"service");
   }

 if(highways & Highways_Track)
   {
    if(*string) strcat(string,", ");
    strcat(string,"track");
   }

 if(highways & Highways_Cycleway)
   {
    if(*string) strcat(string,", ");
    strcat(string,"cycleway");
   }

 if(highways & Highways_Path)
   {
    if(*string) strcat(string,", ");
    strcat(string,"path");
   }

 if(highways & Highways_Steps)
   {
    if(*string) strcat(string,", ");
    strcat(string,"steps");
   }

 if(highways & Highways_Ferry)
   {
    if(*string) strcat(string,", ");
    strcat(string,"ferry");
   }

 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  A string containing the names of allowed transports on a way.

  const char *AllowedNameList Returns the list of names.

  transports_t allowed The allowed type.
  ++++++++++++++++++++++++++++++++++++++*/

const char *AllowedNameList(transports_t allowed)
{
 static char string[256];

 string[0]=0;

 if(allowed & Transports_Foot)
    strcat(string,"foot");

 if(allowed & Transports_Horse)
   {
    if(*string) strcat(string,", ");
    strcat(string,"horse");
   }

 if(allowed & Transports_Wheelchair)
   {
    if(*string) strcat(string,", ");
    strcat(string,"wheelchair");
   }

 if(allowed & Transports_Bicycle)
   {
    if(*string) strcat(string,", ");
    strcat(string,"bicycle");
   }

 if(allowed & Transports_Moped)
   {
    if(*string) strcat(string,", ");
    strcat(string,"moped");
   }

 if(allowed & Transports_Motorcycle)
   {
    if(*string) strcat(string,", ");
    strcat(string,"motorcycle");
   }

 if(allowed & Transports_Motorcar)
   {
    if(*string) strcat(string,", ");
    strcat(string,"motorcar");
   }

 if(allowed & Transports_Goods)
   {
    if(*string) strcat(string,", ");
    strcat(string,"goods");
   }

 if(allowed & Transports_HGV)
   {
    if(*string) strcat(string,", ");
    strcat(string,"hgv");
   }

 if(allowed & Transports_PSV)
   {
    if(*string) strcat(string,", ");
    strcat(string,"psv");
   }

 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  A string containing the names of the properties of a way.

  const char *PropertiesNameList Returns the list of names.

  properties_t properties The properties of the way.
  ++++++++++++++++++++++++++++++++++++++*/

const char *PropertiesNameList(properties_t properties)
{
 static char string[256];

 string[0]=0;

 if(properties & Properties_Paved)
   {
    if(*string) strcat(string,", ");
    strcat(string,"paved");
   }

 if(properties & Properties_Multilane)
   {
    if(*string) strcat(string,", ");
    strcat(string,"multilane");
   }

 if(properties & Properties_Bridge)
   {
    if(*string) strcat(string,", ");
    strcat(string,"bridge");
   }

 if(properties & Properties_Tunnel)
   {
    if(*string) strcat(string,", ");
    strcat(string,"tunnel");
   }

 if(properties & Properties_FootRoute)
   {
    if(*string) strcat(string,", ");
    strcat(string,"footroute");
   }

 if(properties & Properties_BicycleRoute)
   {
    if(*string) strcat(string,", ");
    strcat(string,"bicycleroute");
   }

 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  Returns a list of all the highway types.

  const char *HighwayList Return a list of all the highway types.
  ++++++++++++++++++++++++++++++++++++++*/

const char *HighwayList(void)
{
 return "    motorway     = Motorway\n"
        "    trunk        = Trunk\n"
        "    primary      = Primary\n"
        "    secondary    = Secondary\n"
        "    tertiary     = Tertiary\n"
        "    unclassified = Unclassified\n"
        "    residential  = Residential\n"
        "    service      = Service\n"
        "    track        = Track\n"
        "    cycleway     = Cycleway\n"
        "    path         = Path\n"
        "    steps        = Steps\n"
        "    ferry        = Ferry\n"
        ;
}


/*++++++++++++++++++++++++++++++++++++++
  Returns a list of all the transport types.

  const char *TransportList Return a list of all the transport types.
  ++++++++++++++++++++++++++++++++++++++*/

const char *TransportList(void)
{
 return "    foot       = Foot\n"
        "    bicycle    = Bicycle\n"
        "    wheelchair = Wheelchair\n"
        "    horse      = Horse\n"
        "    moped      = Moped     (Small motorcycle, limited speed)\n"
        "    motorcycle = Motorcycle\n"
        "    motorcar   = Motorcar\n"
        "    goods      = Goods     (Small lorry, van)\n"
        "    hgv        = HGV       (Heavy Goods Vehicle - large lorry)\n"
        "    psv        = PSV       (Public Service Vehicle - bus, coach)\n"
        ;
}


/*++++++++++++++++++++++++++++++++++++++
  Returns a list of all the property types.

  const char *PropertyList Return a list of all the highway proprties.
  ++++++++++++++++++++++++++++++++++++++*/

const char *PropertyList(void)
{
 return "    paved        = Paved (suitable for normal wheels)\n"
        "    multilane    = Multiple lanes\n"
        "    bridge       = Bridge\n"
        "    tunnel       = Tunnel\n"
        "    footroute    = A route marked for foot travel\n"
        "    bicycleroute = A route marked for bicycle travel\n"
        ;
}
