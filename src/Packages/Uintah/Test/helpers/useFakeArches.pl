#!/usr/bin/perl
$srcroot = $ARGV[0];

$filename = "$srcroot" . "/src/Packages/Uintah/StandAlone/sub.mk";

  print "now working on $filename \n";
  open(IN, $filename);
  $changed         = "false";
  $search_str      = ".*sus.cc";
  $replacement_str = "SRCS := \$\(SRCDIR\)/sus.cc
SRCS := \$\(SRCS\) \$\(SRCDIR\)/FakeArches.cc";

  $search_str2     = "
	Packages/Uintah/CCA/Components/Arches .
	Packages/Uintah/CCA/Components/MPMArches .";
  $replacement_str2 = "";
  
  @lines = <IN>;
  $text = join("", @lines);

  if ($text =~ s/$search_str/$replacement_str/g) {
       $changed = "true";
  }
  if ($changed == "true" &&$text =~ s/$search_str2/$replacement_str2/g) {
       $changed = "true";
  }

  close IN;
  print "changed file ",$changed, "\n";
  if ($changed eq "true") {
      open(OUT, "> " . $filename);
      print $filename .  "\n";
      print OUT $text;
      close OUT;
  }

#.............................
# now take care of Components sub.mk
$filename = "$srcroot" . "/src/Packages/Uintah/CCA/Components/sub.mk";

  print "now working on $filename \n";
  open(IN, $filename);
  $changed = "false";
  $search_str       = "
	..SRCDIR./MPMArches .";
  $replacement_str  = "";
  $search_str2      = "
	..SRCDIR./Arches .";
  $replacement_str2 = "";
  @lines = <IN>;
  $text = join("", @lines);

  if ($text =~ s/$search_str/$replacement_str/g) {
    $changed = "true";
  }
  if ($changed == "true" &&$text =~ s/$search_str2/$replacement_str2/g) {
    $changed = "true";
  }
  close IN;
  print "changed file ",$changed, "\n";
  if ($changed eq "true") {
    open(OUT, "> " . $filename);    
    print $filename .  "\n";        
    print OUT $text;                
    close OUT;                      
  }

