#
# Routino generic Search Perl script
#
# Part of the Routino routing software.
#
# This file Copyright 2012 Andrew M. Bishop
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# Use the directory paths script
require "paths.pl";

# Use the perl URI module
use URI::Escape;

# Use the perl LWP module
use LWP::UserAgent;

# Use the perl JSON module
use JSON::PP;

# Use the perl Time::HiRes module
use Time::HiRes qw(gettimeofday tv_interval);

$t0 = [gettimeofday];


#
# Run the search
#

sub RunSearch
  {
   my($search,$left,$right,$top,$bottom)=@_;

   # Perform the search based on the type

   my(@places)=[];

   if($search_type eq "nominatim")
     {
      ($message,@places)=DoNominatimSearch($search,$left,$right,$top,$bottom);
     }
   else
     {
      $message="Unknown search type '$search_type'";
     }

   my(undef,undef,$cuser,$csystem) = times;
   my $time=sprintf "time: %.3f CPU / %.3f elapsed",$cuser+$csystem,tv_interval($t0);

   # Return the results

   return($time,$message,@places);
  }


#
# Fetch the search URL from Nominatim
#

sub DoNominatimSearch
  {
   my($search,$left,$right,$top,$bottom)=@_;

   $search = uri_escape($search);

   my $url;

   if($left && $right && $top && $bottom)
     {
      $url="$search_baseurl?format=json&viewbox=$left,$top,$right,$bottom&q=$search";
     }
   else
     {
      $url="$search_baseurl?format=json&q=$search";
     }

   my $ua=LWP::UserAgent->new;

   my $res=$ua->get($url);

   if(!$res->is_success)
     {
      return($res->status_line);
     }

   my($result)=decode_json($res->content);

   my(@places);

   foreach my $place (@$result)
     {
      my($lat)=$place->{"lat"};
      my($lon)=$place->{"lon"};
      my($name)=$place->{"display_name"};

      push(@places,"$lat $lon $name");
     }

   return("",@places);
  }


1;
