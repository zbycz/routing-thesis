#
# Routino generic router Perl script
#
# Part of the Routino routing software.
#
# This file Copyright 2008-2012 Andrew M. Bishop
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

# Load the profiles variables
require "profiles.pl";

# Use the perl Time::HiRes module
use Time::HiRes qw(gettimeofday tv_interval);

$t0 = [gettimeofday];


#
# Fill in the default parameters using the ones above (don't use executable compiled in defaults)
#

sub FillInDefaults
  {
   my(%params)=@_;

   $params{transport}=$routino->{transport} if(!defined $params{transport});

   my $transport=$params{transport};

   foreach my $highway (keys %{$routino->{highways}})
     {
      my $key="highway-$highway";
      my $value=$routino->{profile_highway}->{$highway}->{$transport};
      $params{$key}=$value if(!defined $params{$key});

      $key="speed-$highway";
      $value=$routino->{profile_speed}->{$highway}->{$transport};
      $params{$key}=$value if(!defined $params{$key});
     }

   foreach my $property (keys %{$routino->{properties}})
     {
      my $key="property-$property";
      my $value=$routino->{profile_property}->{$property}->{$transport};
      $params{$key}=$value if(!defined $params{$key});
     }

   $params{oneway} =~ s/(true|on)/1/;
   $params{oneway} =~ s/(false|off)/0/;

   $params{turns} =~ s/(true|on)/1/;
   $params{turns} =~ s/(false|off)/0/;

   foreach my $restriction (keys %{$routino->{restrictions}})
     {
      my $key="$restriction";
      my $value=$routino->{profile_restrictions}->{$restriction}->{$transport};
      $params{$key}=$value if(!defined $params{$key});
     }

   return %params;
  }


#
# Run the router
#

sub RunRouter
  {
   my($optimise,%params)=@_;

   # Combine all of the parameters together

   my $params="--$optimise";

   foreach my $key (keys %params)
     {
      $params.=" --$key=$params{$key}";
     }

   # Change directory

   mkdir $results_dir,0755 if(! -d $results_dir);
   chdir $results_dir;

   # Create a unique output directory

   my $uuid;

   if($^O eq "darwin")
     {
      chomp($uuid=`echo '$params' $$ | md5    | cut -f1 '-d '`);
     }
   else
     {
      chomp($uuid=`echo '$params' $$ | md5sum | cut -f1 '-d '`);
     }

   mkdir $uuid;
   chmod 0775, $uuid;
   chdir $uuid;

   # Run the router

   my($safe_params)="";
   if($data_dir)
     {
      my(@pathparts)=split('/',$data_dir);
      $safe_params.=" --dir=".pop(@pathparts);
     }
   # This works in newer Perl versions, but not older ones.
   #$safe_params.=" --dir=".pop([split('/',$data_dir)]) if($data_dir);
   $safe_params.=" --prefix=$data_prefix" if($data_prefix);

   open(LOG,">router.log");
   print LOG "$router_exe $params$safe_params\n\n"; # Don't put the full pathnames in the logfile.
   close(LOG);

   $params.=" --dir=$data_dir" if($data_dir);
   $params.=" --prefix=$data_prefix" if($data_prefix);
   $params.=" --loggable";

   system "$bin_dir/$router_exe $params >> router.log 2>&1";

   my $status="OK";
   $status="ERROR" if($? != 0);

   my(undef,undef,$cuser,$csystem) = times;

   open(LOG,">>router.log");
   printf LOG "\nTime: %.3f CPU / %.3f elapsed\n",$cuser+$csystem,tv_interval($t0);
   close(LOG);

   # Return the results

   return($uuid,$status);
  }


#
# Return the output file
#

# Possible file formats

%suffixes=(
           "html"      => ".html",
           "gpx-route" => "-route.gpx",
           "gpx-track" => "-track.gpx",
           "text"      => ".txt",
           "text-all"  => "-all.txt",
           "log"       => ".log"
          );

# Possible MIME types

%mimetypes=(
            "html"      => "text/html",
            "gpx-route" => "text/xml",
            "gpx-track" => "text/xml",
            "text"      => "text/plain",
            "text-all"  => "text/plain",
            "log"       => "text/plain"
           );

sub ReturnOutput
  {
   my($uuid,$type,$format)=@_;

   if($type eq "router") { $format="log" }

   my $suffix=$suffixes{$format};
   my $mime  =$mimetypes{$format};

   my $file="$results_dir/$uuid/$type$suffix";

   # Return the output

   if(!$type || !$uuid || !$format || ! -f $file)
     {
      print header('text/plain','404 Not found');
      print "Not Found!\n";
     }
   else
     {
      print header($mime);

      system "cat $file";
     }
  }

1;
