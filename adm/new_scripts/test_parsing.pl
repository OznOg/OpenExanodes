#!/usr/bin/perl
#
# Test for the parsing of the configuration file
#
package test;
use XML::Simple;
use Data::Dumper;
use commun;

my $format = "%50s %3s %3s %6s\n";
my $error_count = 0;

my $SETCOLOR_SUCCESS= sprintf("%c%s", 27, "[1;32m");
my $SETCOLOR_FAILURE= sprintf("%c%s", 27, "[1;31m");
my $SETCOLOR_WARNING= sprintf("%c%s", 27, "[1;34m");
my $SETCOLOR_NORMAL = sprintf("%c%s", 27, "[0;39m");

sub status_head {
  printf ($format, "Test", "Exp", "Got", "Status");
}

sub status_foot {
  printf ("\n\n");
  if($error_count) {
    print ($SETCOLOR_FAILURE."ERROR: There is $error_count errors left\n".$SETCOLOR_NORMAL);
  }
  else {
    print ($SETCOLOR_SUCCESS."PASSED: All test passed sucessfully\n".$SETCOLOR_NORMAL);
  }

}

sub status {
  my $test = $_[0];
  my $exp  = $_[1];
  my $got  = $_[2];
  my $sts;

  if($exp == $got) {
    $sts = $SETCOLOR_SUCCESS . "PASSED" . $SETCOLOR_NORMAL;
  }  else {
    $sts = $SETCOLOR_FAILURE . "FAILED" . $SETCOLOR_NORMAL;
}
  printf($format, $test, $exp, $got, $sts);

}

sub test_header {
  my $name = $_[0];

  printf ("%10s %-58s %10s\n", "----------",
	  $SETCOLOR_WARNING.$name.$SETCOLOR_NORMAL,
	 "----------");
}

sub test_config {
  my $name = $_[0];
  my $exp  = $_[1];
  my $xml  = $_[2];

  test_header($name);

  my $config = XMLin($xml,
		   forcearray => 1,
		   keyattr => []);
  $error = &commun::verify_config($config);

  $error_count += abs($error-$exp);

  &status($name, $exp, $error);
}

&status_head;

test_config("Cluster name unicity", 1,
	'<driveFUSION>
	  <cluster name="sam"/>
          <cluster name="sam"/>
        </driveFUSION>');

test_config("Cluster / Node name unicity", 1,
	    '<driveFUSION>
               <cluster name="sam">
                 <node name="sam">
                   <device name="disqueATA" path="/dev/hda6" />
                 </node>
                 <node name="sam">
                   <device name="disqueATA" path="/dev/hda6" />
                 </node>
               </cluster>
             </driveFUSION>');

test_config("Cluster /  Node / Device name unicity", 1,
	    '<driveFUSION>
               <cluster name="sam">
                 <node name="sam">
                   <device name="disqueATA"  path="/dev/hda6" />
                   <device name="disqueATA2" path="/dev/hda6" />
                   <device name="disqueATA"  path="/dev/hda6" />
                 </node>
               </cluster>
             </driveFUSION>');

test_config("Device group name unicity", 1,
	'<driveFUSION>
	  <cluster name="sam"/>
          <devicegroup name="video" cluster="sam"/>
          <devicegroup name="video" cluster="sam"/>
        </driveFUSION>');

test_config("Device group / physical unicity", 1,
	'<driveFUSION>
	  <cluster name="sam"/>
          <devicegroup name="video" cluster="sam">
             <physical/>
             <physical/>
          </devicegroup>
        </driveFUSION>');

test_config("Device group / physical / node name unicity", 1,
	'<driveFUSION>
	  <cluster name="sam"/>
          <devicegroup name="video" cluster="sam">
             <physical>
               <device node="sam1" name="disqueATA" />
               <device node="sam1" name="disqueATA" />
               <device node="sam2" name="disqueATA1" />
             </physical>
          </devicegroup>
        </driveFUSION>');

test_config("Device group / logical unicity", 1,
	'<driveFUSION>
	  <cluster name="sam"/>
          <devicegroup name="video" cluster="sam">
             <logical/>
             <logical/>
          </devicegroup>
        </driveFUSION>');

test_config("Device group / logical / device node name unicity", 1,
	'<driveFUSION>
	  <cluster name="sam"/>
          <devicegroup name="video" cluster="sam">
             <logical>
               <device node="sam1" name="disk" sizeMB="123"  />
               <device node="sam1" name="disq"  sizeMB="123"  />
               <device node="sam2" name="disk" sizeMB="123"  />
             </logical>
          </devicegroup>
        </driveFUSION>');

test_config("Device group / logical / device sizeMB is numeric ", 2,
	'<driveFUSION>
	  <cluster name="sam"/>
          <devicegroup name="video" cluster="sam">
             <logical>
               <device node="sam1" name="disk1" sizeMB="123" />
               <device node="sam2" name="disk2" sizeMB="AZ" />
               <device node="sam2" name="disk3" sizeMB="X12" />
             </logical>
          </devicegroup>
        </driveFUSION>');

test_config("Bad cluster name ", 3,
	'<driveFUSION>
	  <cluster name="goodNAME" />
	  <cluster name="A B" />
	  <cluster name="76-RERER FD" />
	  <cluster name="12345678901234567" />
        </driveFUSION>');

#--------------------------------------------------
# common API test
#--------------------------------------------------

#----------------------------------------
# getgrouplist
my $test_name = "getgroupslist";
my $error = 0;
my $exp = 2;

test_header($test_name);

my $config = XMLin('<driveFUSION>
	  <cluster name="sam"/>
          <devicegroup name="video1" cluster="sam" />
          <devicegroup name="video2" cluster="sam1" />
          <devicegroup name="video3" cluster="sam2" />
          <devicegroup name="video4" cluster="sam" />
        </driveFUSION>',
		   forcearray => 1,
		   keyattr => []);

my @group = &commun::getgroupslist2($config, "sam");

if(@group[0] ne "video1") {
  $error++;
  $error_count += abs($error-$exp);
}
&status($test_name . " video1 found", 0, $error);

if(@group[1] ne "video4") {
  $error++;
  $error_count += abs($error-$exp);
}
&status($test_name . " video4 found", 0, $error);

&status($test_name, $exp, $#group + 1);


#--------------------------------------------------
&status_foot;

