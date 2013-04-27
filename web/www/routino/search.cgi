#!/usr/bin/perl
#
# Routino search results retrieval CGI
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

# Use the generic search script
require "search.pl";

# Use the perl CGI module
use CGI ':cgi';


# Create the query and get the parameters

$query=new CGI;

@rawparams=$query->param;

# Legal CGI parameters with regexp validity check

%legalparams=(
              "marker"  => "[0-9]+",

              "left"    => "[-0-9.]+",
              "right"   => "[-0-9.]+",
              "top"     => "[-0-9.]+",
              "bottom"  => "[-0-9.]+",

              "search"  => ".+"
             );

# Validate the CGI parameters, ignore invalid ones

foreach my $key (@rawparams)
  {
   foreach my $test (keys (%legalparams))
     {
      if($key =~ m%^$test$%)
        {
         my $value=$query->param($key);

         if($value =~ m%^$legalparams{$test}$%)
           {
            $cgiparams{$key}=$value;
            last;
           }
        }
     }
  }

# Parse the parameters

$marker=$cgiparams{marker};
$search=$cgiparams{search};

$left  =$cgiparams{left};
$right =$cgiparams{right};
$top   =$cgiparams{top};
$bottom=$cgiparams{bottom};

# Run the search

($search_time,$search_message,@places)=RunSearch($search,$left,$right,$top,$bottom);

# Return the output

print header('text/plain');

print "$marker\n";
print "$search_time\n";
print "$search_message\n";
foreach $place (@places)
  {
   print "$place\n";
  }
