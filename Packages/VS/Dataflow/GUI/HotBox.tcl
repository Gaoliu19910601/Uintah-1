###############################################################################
# File: SCIRun/src/Packages/VS/Dataflow/GUI/HotBox.tcl
#
# Description: TCL UI specification for the HotBox in SCIRun.
#
# Author: Stewart Dickson
###############################################################################


itcl_class VS_DataFlow_HotBox {
  inherit Module
  
  constructor { config } {
    set name HotBox

    set_defaults
  }

  method set_defaults {} {
    global $this-gui_label1
    global $this-gui_label2
    global $this-gui_label3
    global $this-gui_label4
    global $this-gui_label5
    global $this-gui_label6
    global $this-gui_label7
    global $this-gui_label8
    global $this-gui_label9
    global $this-gui_is_injured1
    global $this-gui_is_injured2
    global $this-gui_is_injured3
    global $this-gui_is_injured4
    global $this-gui_is_injured5
    global $this-gui_is_injured6
    global $this-gui_is_injured7
    global $this-gui_is_injured8
    global $this-gui_is_injured9
    global $this-FME_on
    global $this-Files_on
    global $this-enableDraw
    global $this-currentselection
    # values: "fromHotBoxUI" or "fromProbe"
    global $this-selectionsource
    global $this-datafile
    # In: HotBox.cc: #define VS_DATASOURCE_OQAFMA 1
    #                #define VS_DATASOURCE_FILES 2
    global $this-datasource
    global $this-anatomydatasource
    global $this-adjacencydatasource
    global $this-boundingboxdatasource
    global $this-injurylistdatasource
    global $this-querytype
    # The Probe Widget UI
    global $this-gui_probeLocx
    global $this-gui_probeLocy
    global $this-gui_probeLocz
    global $this-gui_probe_scale

    set $this-gui_label1 "label1"
    set $this-gui_label2 "label2"
    set $this-gui_label3 "label3"
    set $this-gui_label4 "label4"
    set $this-gui_label5 "label5"
    set $this-gui_label6 "label6"
    set $this-gui_label7 "label7"
    set $this-gui_label8 "label8"
    set $this-gui_label9 "label9"
    set $this-gui_is_injured1 "0"
    set $this-gui_is_injured2 "0"
    set $this-gui_is_injured3 "0"
    set $this-gui_is_injured4 "0"
    set $this-gui_is_injured5 "0"
    set $this-gui_is_injured6 "0"
    set $this-gui_is_injured7 "0"
    set $this-gui_is_injured8 "0"
    set $this-gui_is_injured9 "0"
    set $this-FME_on "no"
    set $this-Files_on "no"
    set $this-enableDraw "no"
    set $this-currentselection ""
    set $this-datafile ""
    set $this-datasource "2"
    set $this-anatomydatasource ""
    set $this-adjacencydatasource ""
    set $this-boundingboxdatasource ""
    set $this-injurylistdatasource ""
    set $this-querytype "1"
    set $this-gui_probeLocx "0.0"
    set $this-gui_probeLocy "0.0"
    set $this-gui_probeLocz "0.0"
    set $this-gui_probe_scale "1.0"

  }
  # end method set_defaults

  method launch_filebrowser { whichdatasource } {
    set initdir ""
                                                                                
    # place to put preferred data directory
    # it's used if $this-filename is empty
                                                                                
    if {[info exists env(SCIRUN_DATA)]} {
      set initdir $env(SCIRUN_DATA)
    } elseif {[info exists env(SCI_DATA)]} {
      set initdir $env(SCI_DATA)
    } elseif {[info exists env(PSE_DATA)]} {
      set initdir $env(PSE_DATA)
    }

    # extansion to append if no extension supplied by user
    set defext ".csv"

    # file types to appers in filter box
    set types {
        {{Field File}     {.csv}      }
        {{Field File}     {.xml}      }
        {{All Files} {.*}   }
    }
                                                                                
    ######################################################
    # Create a new top-level window for the file browser
                                                                                
    if {$whichdatasource == "anatomy"} {
    set title "Open LabelMap file"
    set $this-anatomydatasource [ tk_getOpenFile \
        -title $title \
        -filetypes $types \
        -initialdir $initdir \
        -defaultextension $defext ]
    if { [set $this-anatomydatasource] != "" } {
         $this-c needexecute
       }
    } elseif {$whichdatasource == "adjacency"} {
    set title "Open Adjacency Map file"
    set $this-adjacencydatasource [ tk_getOpenFile \
        -title $title \
        -filetypes $types \
        -initialdir $initdir \
        -defaultextension $defext ]
    if { [set $this-adjacencydatasource ] != "" } {
         $this-c needexecute
       }
    } elseif {$whichdatasource == "boundingbox"} {
    set title "Open Bounding Box file"
    set $this-boundingboxdatasource [ tk_getOpenFile \
        -title $title \
        -filetypes $types \
        -initialdir $initdir \
        -defaultextension $defext ]
    if { [set $this-boundingboxdatasource ] != "" } {
         $this-c needexecute
       }
    } elseif {$whichdatasource == "injurylist"} {
    set title "Open Injury List file"
    set defext ".xml"
    set $this-injurylistdatasource [ tk_getOpenFile \
        -title $title \
        -filetypes $types \
        -initialdir $initdir \
        -defaultextension $defext ]
    if { [set  $this-injurylistdatasource] != "" } {
         $this-c needexecute
       }
    }
  }
  # end method launch_filebrowser

  method toggle_FME_on {} {
    if {[set $this-FME_on] == "yes"} {
      # toggle FME control off
      set $this-FME_on "no"
    } else {set $this-FME_on "yes"}
  }
  # end method toggle_FME_on

  method toggle_Files_on {} {
    if {[set $this-Files_on] == "yes"} {
      # toggle Files control off
      set $this-Files_on "no"
    } else {set $this-Files_on "yes"}
  }
  # end method toggle_Files_on

  method toggle_enableDraw {} {
    if {[set $this-enableDraw] == "yes"} {
      # toggle HotBox output Geometry off
      set $this-enableDraw "no"
    } else {set $this-enableDraw "yes"}
    # re-execute the module to draw/erase graphics
    $this-c needexecute
  }
  # end method toggle_enableDraw

  method set_selection { selection_id } {
    puts "VS_DataFlow_HotBox::set_selection{$selection_id}"
    switch $selection_id {
	1 {
            set selection [set $this-gui_label1]
	}
	2 {
            set selection [set $this-gui_label2]
	}
	3 {
            set selection [set $this-gui_label3]
	}
	4 {
            set selection [set $this-gui_label4]
	}
	5 {
            set selection [set $this-gui_label5]
	}
	6 {
            set selection [set $this-gui_label6]
	}
	7 {
            set selection [set $this-gui_label7]
	}
	8 {
            set selection [set $this-gui_label8]
	}
	9 {
            set selection [set $this-gui_label9]
	}
        default {
          puts "VS_DataFlow_HotBox::set_selection: not found"
          break
	}
    }
    # end switch $selection_id
    if {[set $this-FME_on] == "yes"} {
      # focus on the selection in the FME
      exec VSgetFME.p $selection
    }
    set $this-currentselection $selection
    # tell the HotBox module that the
    # current selection was changed
    # from the HotBox UI -- not the Probe
    set $this-selectionsource "fromHotBoxUI"
    # trigger the HotBox execution to reflect selection change
    $this-c needexecute
  }
  # end method set_selection

  method set_data_source { datasource } {
    set $this-datasource $datasource
  }
  # end method set_data_source

  method set_querytype { querytype } {
    set $this-querytype $querytype 
  }
  # end method set_querytype

  #############################################################################
  # method ui
  #
  # Build the HotBox UI
  #############################################################################
  method ui {} {
    set w .ui[modname]
    if { [winfo exists $w] } {
      raise $w
      return
    }

    ################################
    # show/hide the data source URIs
    ################################
    toplevel $w
    checkbutton $w.togFilesUI -text "Data Sources" -command "$this toggle_Files_on"
    ################################
    # if selected, show data sources
    ################################
    if { [set $this-Files_on] == "yes" } {
    frame $w.files
    frame $w.files.row1
    label $w.files.row1.anatomylabel -text "Anatomy Data Source: "
    entry $w.files.row1.filenamentry -textvar $this-anatomydatasource -width 50
    button  $w.files.row1.browsebutton -text "Browse..." -command "$this launch_filebrowser anatomy"

    frame $w.files.row2
    label $w.files.row2.adjacencylabel -text "Adjacency Data Source: "
    entry $w.files.row2.filenamentry -textvar $this-adjacencydatasource -width 50
    button  $w.files.row2.browsebutton -text "Browse..." -command "$this launch_filebrowser adjacency"

    frame $w.files.row3
    label $w.files.row3.boundingboxlabel -text "Bounding Box Data Source: "
    entry $w.files.row3.filenamentry -textvar $this-boundingboxdatasource -width 50
    button  $w.files.row3.browsebutton -text "Browse..." -command "$this launch_filebrowser boundingbox"

    frame $w.files.row4
    label $w.files.row4.injurylistlabel -text "Injury List Data Source: "
    entry $w.files.row4.filenamentry -textvar $this-injurylistdatasource -width 50
    button  $w.files.row4.browsebutton -text "Browse..." -command "$this launch_filebrowser injurylist"

    pack $w.files.row1 $w.files.row2 $w.files.row3 $w.files.row4 -side top -anchor w
    pack $w.files.row1.anatomylabel $w.files.row1.filenamentry\
	$w.files.row1.browsebutton\
        -side left -anchor n -expand yes -fill x
    pack $w.files.row2.adjacencylabel $w.files.row2.filenamentry\
	$w.files.row2.browsebutton\
        -side left -anchor n -expand yes -fill x
    pack $w.files.row3.boundingboxlabel $w.files.row3.filenamentry\
	$w.files.row3.browsebutton\
        -side left -anchor n -expand yes -fill x
    pack $w.files.row4.injurylistlabel $w.files.row4.filenamentry\
	$w.files.row4.browsebutton\
        -side left -anchor n -expand yes -fill x
    }
    # end if { [set $this-Files_on] == "yes" }

    frame $w.f
    #############################################################
    # the UI buttons for selecting anatomical names (adjacencies)
    #############################################################
    frame $w.f.row1
    if { [set $this-gui_is_injured1] == "1" } {
    button $w.f.row1.nw -background red -textvariable $this-gui_label1 -command "$this set_selection 1"
    } else {
    button $w.f.row1.nw  -textvariable $this-gui_label1 -command "$this set_selection 1"
    }
    if { [set $this-gui_is_injured2] == "1" } {
    button $w.f.row1.n -background red -textvariable $this-gui_label2 -command "$this set_selection 2"
    } else {
    button $w.f.row1.n   -textvariable $this-gui_label2 -command "$this set_selection 2"
    }
    if { [set $this-gui_is_injured3] == "1" } {
    button $w.f.row1.ne -background red -textvariable $this-gui_label3 -command "$this set_selection 3"
    } else {
    button $w.f.row1.ne  -textvariable $this-gui_label3 -command "$this set_selection 3"
    }
    frame $w.f.row2
    if { [set $this-gui_is_injured4] == "1" } {
    button $w.f.row2.west -background red -textvariable $this-gui_label4 -command "$this set_selection 4"
    } else {
    button $w.f.row2.west -textvariable $this-gui_label4 -command "$this set_selection 4"
    }
    button $w.f.row2.c  -background yellow  -textvariable $this-gui_label5 -command "$this set_selection 5"
    if { [set $this-gui_is_injured6] == "1" } {
    button $w.f.row2.e  -background red -textvariable $this-gui_label6 -command "$this set_selection 6"
    } else {
    button $w.f.row2.e   -textvariable $this-gui_label6 -command "$this set_selection 6"
    }
    frame $w.f.row3
    if { [set $this-gui_is_injured7] == "1" } {
    button $w.f.row3.sw -background red -textvariable $this-gui_label7 -command "$this set_selection 7"
    } else {
    button $w.f.row3.sw  -textvariable $this-gui_label7 -command "$this set_selection 7"
    }
    if { [set $this-gui_is_injured8] == "1" } {
    button $w.f.row3.s  -background red -textvariable $this-gui_label8 -command "$this set_selection 8"
    } else {
    button $w.f.row3.s   -textvariable $this-gui_label8 -command "$this set_selection 8"
    }
    if { [set $this-gui_is_injured9] == "1" } {
    button $w.f.row3.se -background red -textvariable $this-gui_label9 -command "$this set_selection 9"
    } else {
    button $w.f.row3.se  -textvariable $this-gui_label9 -command "$this set_selection 9"
    }

    pack $w.f.row1 $w.f.row2 $w.f.row3 -side top -anchor w
    pack $w.f.row1.nw $w.f.row1.n $w.f.row1.ne\
	-side left -anchor n -expand yes -fill x
    pack $w.f.row2.west $w.f.row2.c $w.f.row2.e\
	-side left -anchor n -expand yes -fill x
    pack $w.f.row3.sw $w.f.row3.s $w.f.row3.se\
        -side left -anchor n -expand yes -fill x

    ######################################
    # Probe UI
    ######################################
    frame $w.probeUI
    frame $w.probeUI.loc
    label $w.probeUI.loc.locLabel -text "Cursor Location" -just left
    entry $w.probeUI.loc.locx -width 10 -textvariable $this-gui_probeLocx
    entry $w.probeUI.loc.locy -width 10 -textvariable $this-gui_probeLocy
    entry $w.probeUI.loc.locz -width 10 -textvariable $this-gui_probeLocz
    bind $w.probeUI.loc.locx <KeyPress-Return> "$this-c needexecute"
    bind $w.probeUI.loc.locy <KeyPress-Return> "$this-c needexecute"
    bind $w.probeUI.loc.locz <KeyPress-Return> "$this-c needexecute"
    pack $w.probeUI.loc.locLabel $w.probeUI.loc.locx $w.probeUI.loc.locy $w.probeUI.loc.locz \
                -side left -anchor n -expand yes -fill x
    scale $w.probeUI.slide -orient horizontal -label "Cursor Size" -from 0 -to 100 -showvalue true \
             -variable $this-gui_probe_scale -resolution 0.25 -tickinterval 25
    set $w.probeUI.slide $this-gui_probe_scale
    bind $w.probeUI.slide <ButtonRelease> "$this-c needexecute"
    bind $w.probeUI.slide <B1-Motion> "$this-c needexecute"
    pack $w.probeUI.slide $w.probeUI.loc -side bottom -expand yes -fill x

    ######################################
    # Query UI
    ######################################
    frame $w.controls
    set ::datasource "2"
    radiobutton $w.controls.adjOQAFMA -value 1 -text "OQAFMA" -variable datasource -command "$this set_data_source 1"
    radiobutton $w.controls.adjFILES -value 2 -text "Files" -variable datasource -command "$this set_data_source 2"

    checkbutton $w.controls.togFME -text "Connect to FME" -command "$this toggle_FME_on"

    checkbutton $w.controls.enableDraw -text "Enable Output Geom" -command "$this toggle_enableDraw"

    ######################################
    # Close the HotBox UI Window
    ######################################
    button $w.controls.close -text "Close" -command "destroy $w"

    pack $w.controls.adjOQAFMA $w.controls.adjFILES $w.controls.togFME $w.controls.enableDraw $w.controls.close -side left -expand yes -fill x

    frame $w.controls2
    label $w.controls2.querytypelabel -text "Query Type: "
    set ::querytype "1"
    radiobutton $w.controls2.adjacenttobutton -value 1 -text "Adjacent to" -variable querytype -command "$this set_querytype 1"
    radiobutton $w.controls2.containsbutton -value 2 -text "Contains" -variable querytype -command "$this set_querytype 2"
    radiobutton $w.controls2.partsbutton -value 3 -text "Parts" -variable querytype -command "$this set_querytype 3"
    radiobutton $w.controls2.partcontainsbutton -value 4 -text "Part Contains" -variable querytype -command "$this set_querytype 4"
    pack $w.controls2.querytypelabel $w.controls2.adjacenttobutton $w.controls2.containsbutton $w.controls2.partsbutton $w.controls2.partcontainsbutton -side left -expand yes -fill x

    if { [set $this-Files_on] == "yes" } {
    pack $w.togFilesUI $w.files $w.f $w.probeUI $w.controls2 $w.controls -side top -expand yes -fill both -padx 5 -pady 5
    } else {
    pack $w.togFilesUI $w.f $w.probeUI $w.controls2 $w.controls -side top -expand yes -fill both -padx 5 -pady 5
    }
# pack $w.title -side top
  }
# end method ui
}
# end itcl_class VS_DataFlow_HotBox
