#  For more information, please see: http://software.sci.utah.edu
# 
#  The MIT License
# 
#  Copyright (c) 2004 Scientific Computing and Imaging Institute,
#  University of Utah.
# 
#  License for the specific language governing rights and limitations under
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
# 
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
# 
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
#

itk::usual Linkedpane {
    keep -background -cursor -sashcursor
}

setProgressText "Loading BioImage Modules, Please Wait..."

#######################################################################
# Check environment variables.  Ask user for input if not set:
# Attempt to get environment variables:
set DATADIR [netedit getenv SCIRUN_DATA]
set DATASET [netedit getenv SCIRUN_DATASET]
#######################################################################

############# NET ##############
::netedit dontschedule
set bbox {0 0 3100 3100}

set m1 [addModuleAtPosition "SCIRun" "Render" "Viewer" 17 2900]


global mods
set mods(Viewer) $m1

set mods(ViewSlices) ""
set mods(EditColorMap2D) ""

# Tooltips
global tips

global new_label
set new_label "Unknown"

global eye
set eye 0

# volume orientations
global top
set top "S"

global front
set front "A"

global side
set side "L"

# show planes
global show_plane_x
global show_plane_y
global show_plane_z
global show_MIP_x
global show_MIP_y
global show_MIP_z
global show_guidelines
set show_plane_x 1
set show_plane_y 1
set show_plane_z 1
set show_MIP_x 0
set show_MIP_y 0
set show_MIP_z 0
set show_guidelines 1
global filter2Dtextures
set filter2Dtextures 1
global planes_mapType
set planes_mapType 0

global slice_frame
set slice_frame(axial) ""
set slice_frame(coronal) ""
set slice_frame(sagittal) ""
set slice_frame(volume) ""
set slice_frame(axial_color) \#1A66FF ;#blue
set slice_frame(coronal_color) \#7FFF1A ;#green
set slice_frame(sagittal_color)  \#CC3366 ;#red





# volume rendering
global show_vol_ren
set show_vol_ren 0
global link_winlevel
set link_winlevel 1
global vol_width
set vol_width 0
global vol_level
set vol_level 0

setProgressText "Loading BioImage Application, Please Wait..."

#######################################################
# Build up a simplistic standalone application.
#######################################################
wm withdraw .

set auto_index(::PowerAppBase) "source [netedit getenv SCIRUN_SRCDIR]/Dataflow/GUI/PowerAppBase.app"

class BioImageApp {
    inherit ::PowerAppBase
        
    constructor {} {
	global mods
	toplevel .standalone
	wm title .standalone "BioImage"	 
	set win .standalone

	# Create insert menu
	menu .standalone.insertmenu -tearoff false -disabledforeground white

	# Set window sizes
	set i_width 260

	set viewer_width 436
	set viewer_height 620 
	
	set notebook_width 260
	set notebook_height [expr $viewer_height - 50]
	
	set process_width 300
	set process_height $viewer_height
	
	set vis_width [expr $notebook_width + 30]
	set vis_height $viewer_height

	set num_filters 0

	set loading_ui 0

	set vis_frame_tab1 ""
	set vis_frame_tab2 ""

	set history0 ""
	set history1 ""

	set dimension 3

	set scolor $execute_color

	# filter indexes
	set filter_type 0
	set modules 1
	set input 2
	set output 3
	set prev_index 4
	set next_index 5
	set choose_port 6
	set which_row 7
	set visibility 8
	set filter_label 9


	set load_choose_input 5
	set load_nrrd 0
	set load_dicom 1
	set load_analyze 2
	set load_field 3
	set load_choose_vis 6
        set load_info 24

	set grid_rows 0

	set label_width 25

	set 0_samples 2
	set 1_samples 2
	set 2_samples 2

        set has_autoviewed 0
        set has_executed 0
        set data_dir ""
        set 2D_fixed 0
	set ViewSlices_executed_on_error 0
        set current_crop -1
	set enter_crop 0

        set turn_off_crop 0
        set updating_crop_ui 0
        set needs_update 1

	set axial-size 0
	set sagittal-size 0
	set coronal-size 0


	set cur_data_tab "Nrrd"
	set c_vis_tab "Planes"

	### Define Tooltips
	##########################
	global tips

    }
    

    destructor {
	destroy $this
    }
    
    
    method appname {} {
	return "BioImage"
    }

    ###########################
    ### indicate_dynamic_compile 
    ###########################
    # Changes the label on the progress bar to dynamic compile
    # message or changes it back
    method indicate_dynamic_compile { which mode } {
 	if {$mode == "start"} {
 	    change_indicate_val 1
 	    change_indicator_labels "Dynamically Compiling [$which name]..."
	} else {
	    change_indicate_val 2
	    if {$executing_modules == 0} {
		if {$grid_rows == 1} {
		    global show_vol_ren
		    if {$show_vol_ren == 1} {
			change_indicator_labels "Done Volume Rendering"
		    } else {
			change_indicator_labels "Done Loading Volume"
		    }		
		} else {
		    change_indicator_labels "Done Updating Pipeline and Visualizing"
		}
	    } else {
		if {$grid_rows == 1} {
		    global show_vol_ren
		    if {$show_vol_ren == 1} {
			change_indicator_labels "Volume Rendering..."
		    } else {
			change_indicator_labels "Loading Volume..."
		    }
		} else {
		    change_indicator_labels "Updating Pipeline and Visualizing..."
		}
	    }
	}
    }
    
    ##########################
    ### indicate_error
    ##########################
    # This method should change the indicator and labels to
    # the error state.  This should be done using the change_indicate_val
    # and change_indicator_labels methods. We catch errors from
    method indicate_error { which msg_state } {

        # disregard UnuCrop errors, hopefully they are due to 
	# upstream crops changing bounds
	if {[string first "UnuCrop" $which] != -1 && ($msg_state == "Warning" \
						      || $msg_state == "Error")} {
	    if {![winfo exists .standalone.cropwarn]} {
		toplevel .standalone.cropwarn
		wm minsize .standalone.cropwarn 150 50
		wm title .standalone.cropwarn "Reset Crop Bounds"
  	        set pos_x [expr $screen_width / 2]
	        set pos_y [expr $screen_height / 2]
		wm geometry .standalone.cropwarn "+$pos_x+$pos_y"

		label .standalone.cropwarn.warn -text "W A R N I N G" \
		    -foreground "#830101"
		label .standalone.cropwarn.message \
		    -text "One or more of your cropping values was out of\nrange. This could be due to recent changes to\nan upstream crop filter's settings affecting a\ndownstream crop filter. The downstream crop\nvalues were reset to the new bounding box and the\ncrop widget was turned off." 
		    
		button .standalone.cropwarn.button -text " Ok " \
		    -command "wm withdraw .standalone.cropwarn" 
		pack .standalone.cropwarn.warn \
		    .standalone.cropwarn.message \
		    .standalone.cropwarn.button \
		    -side top -anchor n -pady 2 -padx 2
	    } else {
		SciRaise .standalone.cropwarn
	    }
	    after 700 "$this stop_crop"
	    return
	}         


	if {$msg_state == "Error"} {
	    if {[string first "ViewSlices" $which] != -1} {
		# hopefully, getting here means that we have new data, and that NrrdInfo
		# has caught it but didn't change slice stuff in time before the ViewSlices
		# module executed
		global mods
		if {$mods(ViewSlices) != ""} {
		    if {$ViewSlices_executed_on_error == 0} {
			set ViewSlices_executed_on_error 1
			after 100 "$mods(ViewSlices)-c needexecute"
		    } elseif {$error_module == ""} {
			set error_module $which
			# turn progress graph red
			change_indicator_labels "E R R O R !"
			change_indicate_val 3
		    }
		}
	    } elseif {$error_module == ""} {
		set error_module $which
		# turn progress graph red
		change_indicator_labels "E R R O R !"
		change_indicate_val 3
	    }
	} else {
	    if {$which == $error_module} {
		set error_module ""
		if {$grid_rows == 1} {
		    global show_vol_ren
		    if {$show_vol_ren == 1} {
			change_indicator_labels "Volume Rendering..."
		    } else {
			change_indicator_labels "Loading Volume..."
		    }
		} else {
		    change_indicator_labels "Updating Pipeline and Visualizing..."
		}
		change_indicate_val 0
		if {[string first "ViewSlices" $which] != -1} {
		    set ViewSlices_executed_on_error 0
		}
	    }
	}
    } 
    ##########################
    ### update_progress
    ##########################
    # This is called when any module calls update_state.
    # We only care about "JustStarted" and "Completed" calls.
    method update_progress { which state } {
	global mods

	if {[string first "NodeGradient" $which] != -1 && $state == "JustStarted"} {
	    change_indicator_labels "Volume Rendering..."
	    change_indicate_val 1
	} elseif {[string first "NodeGradient" $which] != -1 && $state == "Completed"} {
	    change_indicate_val 2
	} elseif {[string first "NrrdTextureBuilder" $which] != -1 && $state == "JustStarted"} {
	    change_indicator_labels "Volume Rendering..."
	    change_indicate_val 1
	} elseif {[string first "NrrdTextureBuilder" $which] != -1 && $state == "Completed"} {
	    change_indicate_val 2
	} elseif {[string first "EditColorMap2D" $which] != -1 && $state == "JustStarted"} {
	    change_indicator_labels "Volume Rendering..."
	    change_indicate_val 1
	} elseif {[string first "EditColorMap2D" $which] != -1 && $state == "Completed"} {
	    change_indicate_val 2
	} elseif {[string first "VolumeVisualizer" $which] != -1 && $state == "JustStarted"} {
	    change_indicator_labels "Volume Rendering..."
	    change_indicate_val 1
        } elseif {[string first "VolumeVisualizer" $which] != -1 && $state == "Completed"} {
	    change_indicate_val 2
	    change_indicator_labels "Done Volume Rendering"
        } elseif {[string first "ViewSlices" $which] != -1 && $state == "Completed"} {
            if {$2D_fixed == 0} {
                # simulate a click in each window and set them to the correct views
		global mods

		# force initial draw in correct modes
		global $mods(ViewSlices)-axial-viewport0-axis
		global $mods(ViewSlices)-sagittal-viewport0-axis
		global $mods(ViewSlices)-coronal-viewport0-axis
 		global $mods(ViewSlices)-axial-viewport0-clut_ww
 		global $mods(ViewSlices)-sagittal-viewport0-clut_ww
 		global $mods(ViewSlices)-coronal-viewport0-clut_ww
 		global $mods(ViewSlices)-axial-viewport0-clut_wl
 		global $mods(ViewSlices)-sagittal-viewport0-clut_wl
 		global $mods(ViewSlices)-coronal-viewport0-clut_wl

                global $mods(ViewSlices)-min $mods(ViewSlices)-max
		set val_min [set $mods(ViewSlices)-min]
		set val_max [set $mods(ViewSlices)-max]

                set ww [expr int([expr abs([expr $val_max-$val_min])])]
                set wl [expr [expr int($ww/2)]]

		set $mods(ViewSlices)-sagittal-viewport0-axis 0
		set $mods(ViewSlices)-coronal-viewport0-axis 1
		set $mods(ViewSlices)-axial-viewport0-axis 2


		set $mods(ViewSlices)-axial-viewport0-clut_ww $ww
		set $mods(ViewSlices)-sagittal-viewport0-clut_ww $ww
		set $mods(ViewSlices)-coronal-viewport0-clut_ww $ww

		set $mods(ViewSlices)-axial-viewport0-clut_wl $wl
		set $mods(ViewSlices)-sagittal-viewport0-clut_wl $wl
		set $mods(ViewSlices)-coronal-viewport0-clut_wl $wl

		$mods(ViewSlices)-c setclut

		global slice_frame
                $mods(ViewSlices)-c rebind $slice_frame(axial).bd.axial
                $mods(ViewSlices)-c rebind $slice_frame(sagittal).bd.sagittal
                $mods(ViewSlices)-c rebind $slice_frame(coronal).bd.coronal


		# rebind 2D windows to call the ViewSlices callback and then BioImage's so we
		# can catch the release
	        
		bind  $slice_frame(axial).bd.axial <ButtonRelease> "$mods(ViewSlices)-c release  %W %b %s %X %Y;" ;#$this update_ViewSlices_button_release %b"
		bind   $slice_frame(sagittal).bd.sagittal <ButtonRelease> "$mods(ViewSlices)-c release  %W %b %s %X %Y" ;# $this update_ViewSlices_button_release %b"
		bind  $slice_frame(coronal).bd.coronal <ButtonRelease> "$mods(ViewSlices)-c release  %W %b %s %X %Y";# $this update_ViewSlices_button_release %b"

		global vol_width vol_level
		set vol_width $ww
		set vol_level $wl

  	        set min [expr $wl-$ww/2]
	        set max [expr $wl+$ww/2]

 		set UnuQuantize [lindex [lindex $filters(0) $modules] 7] 
 		set UnuJhisto [lindex [lindex $filters(0) $modules] 21] 
 		global [set UnuQuantize]-maxf [set UnuQuantize]-minf
 		global [set UnuJhisto]-maxs [set UnuJhisto]-mins

 		set [set UnuQuantize]-maxf $max
 		set [set UnuQuantize]-minf $min
 		set [set UnuJhisto]-maxs "$max nan"
 		set [set UnuJhisto]-mins "$min nan"

		upvar \#0 $mods(ViewSlices)-min min_val 
		upvar \#0 $mods(ViewSlices)-background_threshold thresh
		set thresh $min_val

		# turn off MIP stuff
		after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-MIP Slice0 (1)\}\" 0; $mods(Viewer)-ViewWindow_0-c redraw"
		after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-MIP Slice1 (1)\}\" 0; $mods(Viewer)-ViewWindow_0-c redraw"
		after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-MIP Slice2 (1)\}\" 0; $mods(Viewer)-ViewWindow_0-c redraw"

                set 2D_fixed 1

		# re-execute ViewSlices so that all the planes show up (hack)
                #$mods(ViewSlices)-c needexecute
	    } 

	    global $mods(ViewSlices)-min $mods(ViewSlices)-max

	    $this update_planes_threshold_slider_min_max [set $mods(ViewSlices)-min] [set $mods(ViewSlices)-max]

	} elseif {[string first "Teem_NrrdData_NrrdInfo_1" $which] != -1 && $state == "Completed"} {
	    set axis_num 0
	    global slice_frame
	    foreach axis "sagittal coronal axial" {
		# get Nrrd Dimensions from NrrdInfo Module
		upvar \#0 $which-size$axis_num nrrd_size
		if {![info exists nrrd_size]} return
		set size [expr $nrrd_size - 1]

		$slice_frame($axis).modes.slider.slice.s configure -from 0 -to $size
		$slice_frame($axis).modes.slider.slab.s configure -from 0 -to $size

		set $axis-size $size

		upvar \#0 $mods(ViewSlices)-$axis-viewport0-slice slice
		upvar \#0 $mods(ViewSlices)-$axis-viewport0-slab_min slab_min
		upvar \#0 $mods(ViewSlices)-$axis-viewport0-slab_max slab_max

		if {!$loading} {
		    # set slice to be middle slice
		    set slice [expr $size/2]		;# 50%
		    set slab_min [expr $size/4]		;# 25%
		    set slab_max [expr $size*3/4]	;# 75%
		}
		incr axis_num
	    }
	} elseif {[string first "Teem_NrrdData_NrrdInfo_0" $which] != -1 && $state == "JustStarted"} {
	    change_indicate_val 1
	    change_indicator_labels "Loading Volume..."
	} elseif {[string first "Teem_NrrdData_NrrdInfo_0" $which] != -1 && $state == "Completed"} {
	    change_indicate_val 2
	    set NrrdInfo $which
 	    global [set NrrdInfo]-dimension
 	    set dimension [set [set NrrdInfo]-dimension]

 	    global [set NrrdInfo]-size1
	    
 	    if {[info exists [set NrrdInfo]-size1]} {
 		global [set NrrdInfo]-size0
 		global [set NrrdInfo]-size1
		
 		set 0_samples [set [set NrrdInfo]-size0]
		set 1_samples [set [set NrrdInfo]-size1]

		# configure samples info
 		if {$dimension == 3} {
 		    global [set NrrdInfo]-size2
 		    set 2_samples [set [set NrrdInfo]-size2]
 		    $history0.0.f0.childsite.ui.samples configure -text \
 			"Original Samples: ($0_samples, $1_samples, $2_samples)"
 		    $history1.0.f0.childsite.ui.samples configure -text \
 			"Original Samples: ($0_samples, $1_samples, $2_samples)"
 		} elseif {$dimension == 2} {
 		    $history0.0.f0.childsite.ui.samples configure -text \
 			"Original Samples: ($0_samples, $1_samples)"
 		    $history1.0.f0.childsite.ui.samples configure -text \
 			"Original Samples: ($0_samples, $1_samples)"
 		} else {
 		    puts "ERROR: Only 2D and 3D data supported."
 		    return
 		}
	    }	
	} elseif {[string first "NrrdInfo" $which 0] != -1 && $state == "Completed"} { 
	    # possibly one of the crop NrrdInfos
	    # if it is, set the bounds values in the filters array
            for {set i 1} {$i < $num_filters} {incr i} {
		if {[lindex $filters($i) $filter_type] == "crop" &&
		    [lindex [lindex $filters($i) $modules] 1] == $which} {
		    global $which-size0
		    global $which-size1
		    global $which-size2
		    set bounds_vals [list [expr [set $which-size0]-1] \
					 [expr [set $which-size1]-1] \
					 [expr [set $which-size2]-1]]
		    set filters($i) [lreplace $filters($i) 10 10 $bounds_vals]
		    # set the bounds_set flag to on
		    set filters($i) [lreplace $filters($i) 11 11 1]
		    break
		}
	    }

        } elseif {[string first "UnuResample" $which 0] != -1 && $state == "JustStarted"} {
	    change_indicate_val 1
	    change_indicator_labels "Resampling Volume..."
	} elseif {[string first "UnuResample" $which 0] != -1 && $state == "Completed"} {
	    change_indicate_val 2
	    change_indicator_labels "Done Resampling Volume"
	} elseif {[string first "UnuCrop" $which 0] != -1 && $state == "JustStarted"} {
	    change_indicate_val 1
	    change_indicator_labels "Cropping Volume..."
	} elseif {[string first "UnuCrop" $which 0] != -1 && $state == "Completed"} {
	    change_indicate_val 2
	    change_indicator_labels "Done Cropping Volume"
	} elseif {[string first "UnuHeq" $which 0] != -1 && $which != "Teem_UnuAtoM_UnuHeq_0" && $state == "JustStarted"} {
	    change_indicate_val 1
	    change_indicator_labels "Performing Histogram Equilization..."
	} elseif {[string first "UnuHeq" $which 0] != -1 && $which != "Teem_UnuAtoM_UnuHeq_0" && $state == "Completed"} {
	    change_indicate_val 2
	    change_indicator_labels "Done Performing Histogram Equilization"
	} elseif {[string first "UnuCmedian" $which 0] != -1 && $state == "JustStarted"} {
	    change_indicate_val 1
	    change_indicator_labels "Performing Median/Mode Filtering..."
	} elseif {[string first "UnuCmedian" $which 0] != -1 && $state == "Completed"} {
	    change_indicate_val 2
	    change_indicator_labels "Done Performing Median/Mode Filtering"
	} elseif {[string first "ScalarFieldStats" $which] != -1 && $state == "JustStarted"} {
	    change_indicate_val 1
	    change_indicator_labels "Building Histogram..."
	} elseif {[string first "ScalarFieldStats" $which] != -1 && $state == "Completed"} {
	    change_indicate_val 2
	    change_indicator_labels "Done Building Histogram"
	}
    }
    
    method change_indicate_val { v } {
	# only change an error state if it has been cleared (error_module empty)
	# it will be changed by the indicate_error method when fixed
	if {$indicate != 3 || $error_module == ""} {
	    if {$v == 3} {
		# Error
		set cycle 0
		set indicate 3
		change_indicator
	    } elseif {$v == 0} {
		# Reset
		set cycle 0
		set indicate 0
		change_indicator
	    } elseif {$v == 1} {
		# Start
		set executing_modules [expr $executing_modules + 1]
		set indicate 1
		change_indicator
	    } elseif {$v == 2} {
		# Complete
		set executing_modules [expr $executing_modules - 1]
		if {$executing_modules == 0} {
		    # only change indicator if progress isn't running
		    set indicate 2
		    change_indicator

		    if {$loading} {
			set loading 0
			if {$grid_rows == 1} {
			    global show_vol_ren
			    if {$show_vol_ren == 1} {
				change_indicator_labels "Volume Rendering..."
			    } else {
				change_indicator_labels "Loading Volume..."
			    }
			} else {
			    change_indicator_labels "Updating Pipeline and Visualizing..."
			}
		    }


		    # something wasn't caught, reset
		    set executing_modules 0
		    set indicate 2
		    change_indicator

		    if {$loading} {
			set loading 0
			if {$grid_rows == 1} {
			    global show_vol_ren
			    if {$show_vol_ren == 1} {
				change_indicator_labels "Volume Rendering..."
			    } else {
				change_indicator_labels "Loading Volume..."
			    }			    
			} else {
			    change_indicator_labels "Done Updating Pipeline and Visualizing"
			}		     
		    }

		}
	    }
	}
    }
    
    method change_indicator_labels { msg } {
	$indicatorL0 configure -text $msg
	$indicatorL1 configure -text $msg
    }



    ############################
    ### build_app
    ############################
    # Build the processing and visualization frames and pack along with viewer
    method build_app {d} {
	set data_dir $d
	global mods
	
	wm withdraw .standalone
	# Embed the Viewers

	# add a viewer and tabs to each
	frame $win.viewers

	### Processing Part
	#########################
	### Create Detached Processing Part
	toplevel $win.detachedP
	frame $win.detachedP.f -relief flat
	pack $win.detachedP.f -side left -anchor n -fill both -expand 1
	
	wm title $win.detachedP "Processing Pane"
	
	wm sizefrom $win.detachedP user
	wm positionfrom $win.detachedP user
	
	wm withdraw $win.detachedP


	### Create Attached Processing Part
	frame $win.attachedP 
	frame $win.attachedP.f -relief flat 
	pack $win.attachedP.f -side top -anchor n -fill both -expand 1

	set IsPAttached 1

	### set frame data members
	set detachedPFr $win.detachedP
	set attachedPFr $win.attachedP

	init_Pframe $detachedPFr.f 0
	init_Pframe $attachedPFr.f 1

	#change_current 0

	### create detached width and heigh
	append geomP $process_width x $process_height
	wm geometry $detachedPFr $geomP


	### Vis Part
	#####################
	### Create a Detached Vis Part
	toplevel $win.detachedV
	frame $win.detachedV.f -relief flat
	pack $win.detachedV.f -side left -anchor n

	wm title $win.detachedV "Visualization Settings Pane"

	wm sizefrom $win.detachedV user
	wm positionfrom $win.detachedV user
	
	wm withdraw $win.detachedV

	### Create Attached Vis Part
	frame $win.attachedV
	frame $win.attachedV.f -relief flat
	pack $win.attachedV.f -side left -anchor n -fill both

	set IsVAttached 1

	### set frame data members
	set detachedVFr $win.detachedV
	set attachedVFr $win.attachedV
	
	init_Vframe $detachedVFr.f 1
	init_Vframe $attachedVFr.f 2


	### pack 3 frames
	pack $attachedPFr -side left -anchor n -fill y

	pack $win.viewers -side left -anchor n -fill both -expand 1

	pack $attachedVFr -side left -anchor n -fill y

	set total_width [expr $process_width + $viewer_width + $vis_width]

	set total_height $viewer_height

	set pos_x [expr [expr $screen_width / 2] - [expr $total_width / 2]]
	set pos_y [expr [expr $screen_height / 2] - [expr $total_height / 2]]

	append geom $total_width x $total_height + $pos_x + $pos_y
	wm geometry .standalone $geom
	update	

        set initialized 1

	global PowerAppSession
	if {[info exists PowerAppSession] && [set PowerAppSession] != ""} { 
	    set saveFile $PowerAppSession
	    wm title .standalone "BioImage - [getFileName $saveFile]"
	    $this load_session_data
	} 
	wm deiconify .standalone
    }

    method old_build_viewers {viewer viewimage} {
	set w $win.viewers
	
	global mods
	set mods(ViewSlices) $viewimage

	iwidgets::panedwindow $w.topbot -orient horizontal -thickness 0 \
	    -sashwidth 5000 -sashindent 0 -sashborderwidth 2 -sashheight 6 \
	    -sashcursor sb_v_double_arrow -width $viewer_width -height $viewer_height
	pack $w.topbot -expand 1 -fill both -padx 0 -ipadx 0 -pady 0 -ipady 0
	
	$w.topbot add top -margin 0 -minimum 0
	$w.topbot add bottom  -margin 0 -minimum 0

	set top [$w.topbot childsite top]
	set bot [$w.topbot childsite bottom]

	Linkedpane $top.lr -orient vertical -thickness 0 \
	    -sashheight 5000 -sashwidth 6 -sashindent 0 -sashborderwidth 2 \
	    -sashcursor sb_h_double_arrow

	$top.lr add left -margin 3 -minimum 0
	$top.lr add right -margin 3 -minimum 0
	set topl [$top.lr childsite left]
	set topr [$top.lr childsite right]

	Linkedpane $bot.lr  -orient vertical -thickness 0 \
	    -sashheight 5000 -sashwidth 6 -sashindent 0 -sashborderwidth 2 \
	    -sashcursor sb_h_double_arrow

	$bot.lr set_link $top.lr
	$top.lr set_link $bot.lr

	$bot.lr add left -margin 3 -minimum 0
	$bot.lr add right -margin 3 -minimum 0
	set botl [$bot.lr childsite left]
	set botr [$bot.lr childsite right]

	pack $top.lr -expand 1 -fill both -padx 0 -ipadx 0 -pady 0 -ipady 0
	pack $bot.lr -expand 1 -fill both -padx 0 -ipadx 0 -pady 0 -ipady 0

	$viewimage control_panel $w.cp
	$viewimage add_nrrd_tab $w 1
	global slice_frame
	set slice_frame(3d) $topl
	set slice_frame(axial) $topr
	set slice_frame(coronal) $botr
	set slice_frame(sagittal) $botl
	
	foreach axis "sagittal coronal axial" {
	    create_2d_frame $slice_frame($axis) $axis
	    $viewimage gl_frame $slice_frame($axis).$axis
	    pack $slice_frame($axis).$axis \
		-side top -padx 0 -ipadx 0 -pady 0 -ipady 0
	}

	# embed viewer in top left
	global mods
 	set eviewer [$mods(Viewer) ui_embedded]

 	$eviewer setWindow $topl [expr $viewer_width/2] \
 	    [expr $viewer_height/2] \

 	pack $topl -side top -anchor n \
 	    -expand 1 -fill both -padx 0 -pady 0

    }


    method build_viewers {viewer viewimage} {
	set w $win.viewers
	
	global mods
	set mods(ViewSlices) $viewimage

	iwidgets::panedwindow $w.topbot -orient horizontal -thickness 0 \
	    -sashwidth 5000 -sashindent 0 -sashborderwidth 2 -sashheight 6 \
	    -sashcursor sb_v_double_arrow -width $viewer_width -height $viewer_height
	pack $w.topbot -expand 1 -fill both -padx 0 -ipadx 0 -pady 0 -ipady 0
#	Tooltip $w.topbot "Click and drag to resize"
	
	$w.topbot add top -margin 3 -minimum 0
	$w.topbot add bottom  -margin 0 -minimum 0

	set bot [$w.topbot childsite top]
	set top [$w.topbot childsite bottom]

	$w.topbot fraction 62 38
	iwidgets::panedwindow $top.lmr -orient vertical -thickness 0 \
	    -sashheight 5000 -sashwidth 6 -sashindent 0 -sashborderwidth 2 \
	    -sashcursor sb_h_double_arrow
#	Tooltip $top.lmr "Click and drag to resize"

	$top.lmr add left -margin 3 -minimum 0
	$top.lmr add middle -margin 3 -minimum 0
	$top.lmr add right -margin 3 -minimum 0
	set topl [$top.lmr childsite left]
	set topm [$top.lmr childsite middle]
	set topr [$top.lmr childsite right]

	pack $top.lmr -expand 1 -fill both -padx 0 -ipadx 0 -pady 0 -ipady 0

	$viewimage control_panel $w.cp
	$viewimage add_nrrd_tab $w 1
	global slice_frame
	set slice_frame(3d) $bot
	set slice_frame(sagittal) $topl
	set slice_frame(coronal) $topm
	set slice_frame(axial) $topr

	foreach axis "sagittal coronal axial" {
	    global $mods(ViewSlices)-$axis-viewport0-mode
	    create_2d_frame $slice_frame($axis) $axis
	    frame $slice_frame($axis).bd -bd 1 \
		-background $slice_frame(${axis}_color)
	    pack $slice_frame($axis).bd -expand 1 -fill both \
		-side top -padx 0 -ipadx 0 -pady 0 -ipady 0
	    $viewimage gl_frame $slice_frame($axis).bd.$axis
	    pack $slice_frame($axis).bd.$axis -expand 1 -fill both \
		-side top -padx 0 -ipadx 0 -pady 0 -ipady 0
	}

	# embed viewer in top left
	global mods
 	set eviewer [$mods(Viewer) ui_embedded]

 	$eviewer setWindow $slice_frame(3d) [expr $viewer_width/2] \
 	    [expr $viewer_height/2] \

 	pack $slice_frame(3d) -side top -anchor n \
 	    -expand 1 -fill both -padx 4 -pady 0

    }

    method create_2d_frame { window axis } {
	# Modes for $axis
	frame $window.modes
	pack $window.modes -side bottom -padx 0 -pady 0 -expand 0 -fill x
	
	frame $window.modes.buttons
	frame $window.modes.slider
	pack $window.modes.buttons $window.modes.slider \
	    -side top -pady 0 -anchor n -expand yes -fill x

	global mods slice_frame
	
	# Radiobuttons
	radiobutton $window.modes.buttons.slice -text "Slice" \
	    -variable $mods(ViewSlices)-$axis-viewport0-mode -value 0 \
	    -command "$this update_ViewSlices_mode $axis"
	Tooltip $window.modes.buttons.slice "Select to view in\nsingle slice mode.\nAdjust slider to\nchange current\nslice"
	radiobutton $window.modes.buttons.slab -text "Slab" \
	    -variable $mods(ViewSlices)-$axis-viewport0-mode -value 1 \
	    -command "$this update_ViewSlices_mode $axis"
	Tooltip $window.modes.buttons.slab "Select to view a\nmaximum intensity\nprojection of a slab\nof slices"
	radiobutton $window.modes.buttons.mip -text "MIP" \
	    -variable $mods(ViewSlices)-$axis-viewport0-mode -value 2 \
	    -command "$this update_ViewSlices_mode $axis"
	Tooltip $window.modes.buttons.mip "Select to view a\nmaximum intensity\nprojection of all\nslices"
	pack $window.modes.buttons.slice $window.modes.buttons.slab \
	    $window.modes.buttons.mip -side left -anchor n -padx 2 \
	    -expand yes -fill x
	
	# Initialize with slice scale visible
	frame $window.modes.slider.slice
	pack $window.modes.slider.slice -side top -anchor n -expand 1 -fill x

	# slice slider
	scale $window.modes.slider.slice.s \
	    -variable $mods(ViewSlices)-$axis-viewport0-slice \
	    -from 0 -to 20 -width 15 \
	    -showvalue false \
	    -orient horizontal \
	    -command "$mods(ViewSlices)-c rebind $slice_frame($axis).bd.$axis; \
                      $mods(ViewSlices)-c redrawall"

	# slice value label
	entry $window.modes.slider.slice.l \
	    -textvariable $mods(ViewSlices)-$axis-viewport0-slice \
	    -justify left -width 3
	bind $window.modes.slider.slice.l <Return>  "$mods(ViewSlices)-c rebind $slice_frame($axis).bd.$axis; $mods(ViewSlices)-c redrawall"

	
	pack $window.modes.slider.slice.l -anchor e -side right \
	    -padx 0 -pady 0 -expand 0

	pack $window.modes.slider.slice.s -anchor n -side left \
	    -padx 0 -pady 0 -expand 1 -fill x

	
	# Create range widget for slab mode
	frame $window.modes.slider.slab
	# min range value label
	entry $window.modes.slider.slab.min \
	    -textvariable $mods(ViewSlices)-$axis-viewport0-slab_min \
	    -justify right -width 3 
	bind $window.modes.slider.slab.min <Return> "$mods(ViewSlices)-c rebind $slice_frame($axis).bd.$axis; $mods(ViewSlices)-c redrawall" 
	# MIP slab range widget
	range $window.modes.slider.slab.s -from 0 -to 20 \
	    -orient horizontal -showvalue false \
	    -rangecolor "#830101" -width 15 \
	    -varmin $mods(ViewSlices)-$axis-viewport0-slab_min \
	    -varmax $mods(ViewSlices)-$axis-viewport0-slab_max \
	    -command "$mods(ViewSlices)-c rebind $slice_frame($axis).bd.$axis; \
                      $mods(ViewSlices)-c redrawall"
	Tooltip $window.modes.slider.slab.s "Click and drag the\nmin or max sliders\nto change the extent\nof the slab. Click\nand drage the red\nrange bar to change the\ncenter poisition of\nthe slab range"
	# max range value label
	entry $window.modes.slider.slab.max \
	    -textvariable $mods(ViewSlices)-$axis-viewport0-slab_max \
	    -justify left -width 3
	bind $window.modes.slider.slab.max <Return> "$mods(ViewSlices)-c rebind $slice_frame($axis).bd.$axis; $mods(ViewSlices)-c redrawall" 
	
	pack $window.modes.slider.slab.min -anchor w -side left \
	    -padx 0 -pady 0 -expand 0 

	pack $window.modes.slider.slab.max -anchor e -side right \
	    -padx 0 -pady 0 -expand 0 

	pack $window.modes.slider.slab.s \
	    -side left -anchor n -padx 0 -pady 0 -expand 1 -fill x

	# show/hide bar
	set img [image create photo -width 1 -height 1]
	button $window.modes.expand -height 4 -bd 2 \
	    -relief raised -image $img \
	    -cursor based_arrow_down \
	    -command "$this hide_control_panel $window.modes"
	Tooltip $window.modes.expand "Click to minimize/show the\nviewing mode controls"
	pack $window.modes.expand -side bottom -fill both
    }
    

    method show_control_panel { w } {
	pack forget $w.expand
	pack $w.buttons $w.slider -side top -pady 0 -anchor nw -expand yes -fill x
	pack $w.expand -side bottom -fill both

	$w.expand configure -command "$this hide_control_panel $w" \
	    -cursor based_arrow_down
    }

    method hide_control_panel { w } {
	pack forget $w.buttons $w.slider
	pack $w.expand -side bottom -fill both

	$w.expand configure -command "$this show_control_panel $w" \
	    -cursor based_arrow_up
    }

    method update_ViewSlices_button_release {b} {
 	if {$b == 1} {
 	    # Window/level just changed
 	    global link_winlevel
 	    if {$link_winlevel == 1} {
		global mods vol_width vol_level
		global $mods(ViewSlices)-axial-viewport0-clut_ww 
		global $mods(ViewSlices)-axial-viewport0-clut_wl
		
		set ww [set $mods(ViewSlices)-axial-viewport0-clut_ww]
		set wl [set $mods(ViewSlices)-axial-viewport0-clut_wl]
		set min [expr $wl-$ww/2]
		set max [expr $wl+$ww/2]

		set vol_width $ww
		set vol_level $wl
		
 		# Update the UnuQuantize min/max and the 
 		# UnuJhisto axis 0 mins/maxs
 		set UnuQuantize [lindex [lindex $filters(0) $modules] 7] 
 		set UnuJhisto [lindex [lindex $filters(0) $modules] 21] 
		
 		global [set UnuQuantize]-maxf [set UnuQuantize]-minf
 		global [set UnuJhisto]-maxs [set UnuJhisto]-mins
		
 		set [set UnuQuantize]-maxf $max
 		set [set UnuQuantize]-minf $min
		
 		set [set UnuJhisto]-maxs "$max nan"
 		set [set UnuJhisto]-mins "$min nan"

		# execute modules if volume rendering enabled
		global show_vol_ren
		if {$show_vol_ren == 1} {
		    [set UnuQuantize]-c needexecute
		    [set UnuJhisto]-c needexecute
		}
 	    }

 	    # update background threshold slider min/max 
	    $mods(ViewSlices)-c background_thresh
 	}
    }

    #############################
    ### init_Pframe
    #############################
    # Initialize the processing frame on the left. For this app
    # that includes the Load Volume, Restriation, and Build Tensors steps.
    # This method will call the base class build_menu method and sets 
    # the variables that point to the tabs and tabnotebooks.
    method init_Pframe { m case } {
        global mods
	global tips
        
	if { [winfo exists $m] } {

	    build_menu $m

	    frame $m.p -borderwidth 2 -relief groove
	    pack $m.p -side left -fill both -anchor nw -expand yes

	    ### Filter Menu
	    frame $m.p.filters 
	    pack $m.p.filters -side top -expand no -anchor n -pady 1

	    set filter $m.p.filters
	    button $filter.resamp -text "Resample" \
		-background $scolor -padx 3 \
		-activebackground "#6c90ce" \
		-command "$this add_Resample -1"
	    Tooltip $filter.resamp "Resample using UnuResample"

	    button $filter.crop -text "Crop" \
		-background $scolor -padx 3 \
		-activebackground "#6c90ce" \
		-command "$this add_Crop -1"
	    Tooltip $filter.crop "Crop the image"

	    button $filter.cmedian -text "Cmedian" \
		-background $scolor -padx 3 \
		-activebackground "#6c90ce" \
		-command "$this add_Cmedian -1"
	    Tooltip $filter.cmedian "Median/mode filtering"

	    button $filter.histo -text "Histogram" \
		-background $scolor -padx 3 \
		-activebackground "#6c90ce" \
		-command "$this add_Histo -1"
	    Tooltip $filter.histo "Perform Histogram Equilization\nusing UnuHeq"

	    pack $filter.resamp $filter.crop $filter.histo $filter.cmedian \
		-side left -padx 2 -expand no

	    iwidgets::scrolledframe $m.p.sf -width [expr $process_width - 20] \
		-height [expr $process_height - 180] -labeltext "History"
	    pack $m.p.sf -side top -anchor nw -expand yes -fill both
	    set history [$m.p.sf childsite]

	    Tooltip $history "Shows a history of steps\nin the dynamic pipeline"

	    # Add Load UI
	    $this add_Load $history $case
	    
	    set grid_rows 1
	    set num_filters 1	 	 

	    set history$case $history

	    button $m.p.update -text "U p d a t e" \
		-command "$this update_changes" \
		-background "#008b45" \
		-activebackground "#31a065"
	    Tooltip $m.p.update "Update any filter changes, also\nupdating the currenlty viewed data."

	    pack $m.p.update -side top -anchor s -padx 3 -pady 3 -ipadx 3 -ipady 2

	    
            ### Indicator
	    frame $m.p.indicator -relief sunken -borderwidth 2
            pack $m.p.indicator -side bottom -anchor s -padx 3 -pady 5
	    
	    canvas $m.p.indicator.canvas -bg "white" -width $i_width \
	        -height $i_height 
	    pack $m.p.indicator.canvas -side top -anchor n -padx 3 -pady 3
	    
            bind $m.p.indicator <Button> {app display_module_error} 
	    
            label $m.p.indicatorL -text "Press Update to Load Volume..."
            pack $m.p.indicatorL -side bottom -anchor sw -padx 5 -pady 3
	    
	    set indicator$case $m.p.indicator.canvas
	    set indicatorL$case $m.p.indicatorL

	    Tooltip $m.p.indicatorL $tips(IndicatorLabel)
	    
            construct_indicator $m.p.indicator.canvas
	    
	    
	    ### Attach/Detach button
            frame $m.d 
	    pack $m.d -side left -anchor e
            for {set i 0} {$i<40} {incr i} {
                button $m.d.cut$i -text " | " -borderwidth 0 \
                    -foreground "gray25" \
                    -activeforeground "gray25" \
                    -command "$this switch_P_frames" 
	        pack $m.d.cut$i -side top -anchor se -pady 0 -padx 0
                if {$case == 0} {
		    Tooltip $m.d.cut$i $tips(ProcAttachHashes)
		} else {
		    Tooltip $m.d.cut$i $tips(ProcDetachHashes)
		}
            }
	    
	}
	
        wm protocol .standalone WM_DELETE_WINDOW { NiceQuit }  
	
    }

    
    # Method to create Load/Vis modules, and build
    # Load UI.  Variable m gives the path and case
    # indicates whether it is being built for the attached
    # or detached frame.  The modules only need to be created
    # once, so when case equals 0, create modules and ui, for case 1
    # just create ui.
    method add_Load {history case} {

	# if first time in this method (case == 0)
	# create the modules and connections
	if {$case == 0} {
	    global mods
	    
	    # create load modules and inner connections
	    set m1 [addModuleAtPosition "Teem" "DataIO" "NrrdReader" 10 10]
	    set m2 [addModuleAtPosition "Teem" "DataIO" "DicomNrrdReader" 28 70]
	    set m3 [addModuleAtPosition "Teem" "DataIO" "AnalyzeNrrdReader" 46 128]
	    set m4 [addModuleAtPosition "SCIRun" "DataIO" "FieldReader" 65 186]
	    set m5 [addModuleAtPosition "Teem" "DataIO" "FieldToNrrd" 65 245]
	    set m6 [addModuleAtPosition "Teem" "NrrdData" "ChooseNrrd" 10 324]
	    set m25 [addModuleAtPosition "Teem" "NrrdData" "NrrdInfo" 65 1054]
	    	  
	    set c1 [addConnection $m4 0 $m5 0]
	    set c2 [addConnection $m1 0 $m6 0]
	    set c3 [addConnection $m2 0 $m6 1]
	    set c4 [addConnection $m3 0 $m6 2]
	    set c5 [addConnection $m5 2 $m6 3]

	    
	    # Disable other load modules (Dicom, Analyze, Field)
	    disableModule $m2 1
	    disableModule $m3 1
	    disableModule $m4 1
	    
	    # create vis modules and inner connections
	    set m7 [addModuleAtPosition "Teem" "NrrdData" "ChooseNrrd" 10 1900]
	    set m8 [addModuleAtPosition "Teem" "UnuNtoZ" "UnuQuantize" 10 2191]
	    set m9 [addModuleAtPosition "Teem" "UnuNtoZ" "UnuQuantize" 517 2215]
	    set m10 [addModuleAtPosition "Teem" "UnuAtoM" "UnuJoin" 218 2278]
	    set m11 [addModuleAtPosition "Teem" "UnuNtoZ" "UnuQuantize" 218 2198]
	    set m12 [addModuleAtPosition "SCIRun" "Visualization" "NrrdTextureBuilder" 182 2674]
	    set m13 [addModuleAtPosition "Teem" "UnuNtoZ" "UnuProject" 447 2138]
	    set m14 [addModuleAtPosition "SCIRun" "Visualization" "EditColorMap2D" 375 2675]
	    set m15 [addModuleAtPosition "SCIRun" "Visualization" "VolumeVisualizer" 224 2760]
	    set m16 [addModuleAtPosition "Teem" "DataIO" "NrrdToField" 182 1937]
	    set m17 [addModuleAtPosition "SCIRun" "FieldsData" "NodeGradient" 182 1997]
	    set m18 [addModuleAtPosition "Teem" "DataIO" "FieldToNrrd" 182 2056]
	    set m19 [addModuleAtPosition "Teem" "UnuAtoM" "UnuHeq" 392 2473]
	    set m20 [addModuleAtPosition "Teem" "UnuAtoM" "UnuGamma" 392 2535]
	    set m21 [addModuleAtPosition "Teem" "UnuNtoZ" "UnuQuantize" 392 2597]
	    set m22 [addModuleAtPosition "Teem" "UnuAtoM" "UnuJhisto" 410 2286]
	    set m23 [addModuleAtPosition "Teem" "UnuAtoM" "Unu2op" 392 2348]
	    set m24 [addModuleAtPosition "Teem" "UnuAtoM" "Unu1op" 392 2409]
            set m26 [addModuleAtPosition "SCIRun" "Render" "ViewSlices" 704 2057]
	    set m27 [addModuleAtPosition "SCIRun" "Visualization" "GenStandardColorMaps" 741 1977]
	    set m28 [addModuleAtPosition "Teem" "NrrdData" "NrrdInfo" 369 1889]

	    set m29 [addModuleAtPosition "Teem" "UnuNtoZ" "UnuPermute" 81 408]
	    set m30 [addModuleAtPosition "Teem" "UnuAtoM" "UnuFlip" 81 575]
	    set m31 [addModuleAtPosition "Teem" "UnuAtoM" "UnuFlip" 60 731]
	    set m32 [addModuleAtPosition "Teem" "UnuAtoM" "UnuFlip" 50 892]
	    set m33 [addModuleAtPosition "Teem" "NrrdData" "ChooseNrrd" 9 653]
	    set m34 [addModuleAtPosition "Teem" "NrrdData" "ChooseNrrd" 8 813]
	    set m35 [addModuleAtPosition "Teem" "NrrdData" "ChooseNrrd" 8 970]
	    set m36 [addModuleAtPosition "Teem" "NrrdData" "ChooseNrrd" 10 485]
	    set m37 [addModuleAtPosition "Teem" "NrrdData" "ChooseNrrd" 183 2398]


	    # store some in mods
	    set mods(EditColorMap2D) $m14
	    
	    set c1 [addConnection $m17 0 $m18 0]
	    set c2 [addConnection $m12 0 $m15 0]
	    set c3 [addConnection $m18 2 $m13 0]
	    set c4 [addConnection $m18 2 $m11 0]
	    set c5 [addConnection $m16 0 $m17 0]
	    set c6 [addConnection $m24 0 $m19 0]
	    set c7 [addConnection $m23 0 $m24 0]
	    set c8 [addConnection $m20 0 $m21 0]
	    set c9 [addConnection $m19 0 $m20 0]
	    set c10 [addConnection $m8 0 $m37 0]
	    set c11 [addConnection $m13 0 $m9 0]
	    set c12 [addConnection $m11 0 $m10 0]
	    set c13 [addConnection $m22 0 $m23 1]
	    set c14 [addConnection $m8 0 $m10 1]
	    set c15 [addConnection $m9 0 $m12 1]
	    set c16 [addConnection $m21 0 $m14 1]
	    set c17 [addConnection $m14 0 $m15 2]
	    set c18 [addConnection $m13 0 $m22 2]
	    set c19 [addConnection $m7 0 $m26 0]
	    set c20 [addConnection $m27 0 $m26 2]
	    set c29 [addConnection $m7 0 $m28 0]

	    # connect load to vis
	    set c21 [addConnection $m6 0 $m36 0]
	    set c22 [addConnection $m7 0 $m8 0]
	    set c23 [addConnection $m7 0 $m16 2]
	    set c24 [addConnection $m7 0 $m22 1]

	    # connect vis to Viewer
	    set c25 [addConnection $m15 0 $mods(Viewer) 0]

	    # flip connections
	    # might want to connect this to $m6 instead of $m36
	    # depending on desired behavior

 	    set c26 [addConnection $m33 0 $m34 0]
 	    set c27 [addConnection $m33 0 $m31 0]
 	    set c28 [addConnection $m34 0 $m35 0]
 	    set c29 [addConnection $m34 0 $m32 0]
            set c32 [addConnection $m6 0 $m29 0]
            set c33 [addConnection $m6 0 $m36 0]
            set c34 [addConnection $m29 0 $m36 1]
	    set c35 [addConnection $m36 0 $m30 0]
	    set c36 [addConnection $m36 0 $m33 0]
	    set c36 [addConnection $m30 0 $m33 1]
	    set c38 [addConnection $m31 0 $m34 1]
	    set c39 [addConnection $m32 0 $m35 1]
	    set c40 [addConnection $m35 0 $m25 0]
	    set c41 [addConnection $m35 0 $m7 0]

	    # connect 2D Viewer to 3D Viewer
	    set c37 [addConnection $m26 0 $mods(Viewer) 1]

	    # connect EditColorMap2D to ViewSlices for painting
	    set c42 [addConnection $m14 0 $m26 4]

	    # connect Gradient Magnitude to ViewSlices for painting
	    set c43 [addConnection $m9 0 $m26 5]

	    # more connections
	    set c44 [addConnection $m10 0 $m37 1]
	    set c45 [addConnection $m37 0 $m12 0]

	    set c46 [addConnection $m26 1 $m14 0]

	    # disable the volume rendering
 	    disableModule $m8 1
 	    disableModule $m15 1
 	    disableModule $m17 1
 	    disableModule $m18 1
 	    disableModule $m22 1
            disableModule $m14 1
	    disableModule $m12 1
	    disableModule $m13 1

	    # disable flip/permute modules
	    disableModule $m29 1
	    disableModule $m30 1
	    disableModule $m31 1
	    disableModule $m32 1

	    # set some ui parameters
	    setGlobal $m1-filename $data_dir/volume/tooth.nhdr

	    setGlobal $m8-nbits {8}
	    setGlobal $m8-useinputmin 0
	    setGlobal $m8-useinputmax 0

	    setGlobal $m9-nbits {8}
	    setGlobal $m9-useinputmin 1
	    setGlobal $m9-useinputmax 1

	    setGlobal $m11-nbits {8}
	    setGlobal $m11-useinputmin 1
	    setGlobal $m11-useinputmax 1

	    setGlobal $m13-measure {9}

	    # CHANGE THESE VARS FOR TRANSFER FUNCTION 
	    setGlobal $m14-faux {1}
	    setGlobal $m14-histo {0.5}
	    setGlobal $m14-num-entries {4}
	    setGlobal $m14-name-0 {Dentin/Pulp}
	    setGlobal $m14-0-color-r {0.8}
	    setGlobal $m14-0-color-g {0.17}
	    setGlobal $m14-0-color-b {0.17}
	    setGlobal $m14-0-color-a {1.0}
	    setGlobal $m14-state-0 \
		{r 0 0.0429688 0.335938 0.367187 0.453125 0.427807}
	    setGlobal $m14-on-0 {1}
	    setGlobal $m14-name-1 {Bone/Dentin}
	    setGlobal $m14-1-color-r {0.82}
	    setGlobal $m14-1-color-g {0.84}
	    setGlobal $m14-1-color-b {0.33}
	    setGlobal $m14-1-color-a {0.600000023842}
	    setGlobal $m14-state-1 \
		{t 0.462891 0.0679688 0.554687 0.295832 0.304965}
	    setGlobal $m14-on-1 {1}
	    setGlobal $m14-name-2 {Dentin/Enamel}
	    setGlobal $m14-2-color-r {0.38}
	    setGlobal $m14-2-color-g {0.4}
	    setGlobal $m14-2-color-b {1.0}
	    setGlobal $m14-2-color-a {0.72000002861}
	    setGlobal $m14-state-2 \
		{r 0 0.607422 0.222656 0.277344 0.300781 0.465753}
	    setGlobal $m14-on-2 {1}
	    setGlobal $m14-name-3 {Air/Enamel}
	    setGlobal $m14-3-color-r {1.0}
	    setGlobal $m14-3-color-g {1.0}
	    setGlobal $m14-3-color-b {1.0}
	    setGlobal $m14-3-color-a {0.810000002384}
	    setGlobal $m14-state-3 \
		{r 0 0.443359 0.722656 0.367188 0.253906 0.515625}
	    setGlobal $m14-on-3 {1}
	    setGlobal $m14-marker {end}

	    setGlobal $m15-alpha_scale {0.0}
	    setGlobal $m15-shading {1}
	    setGlobal $m15-ambient {0.5}
	    setGlobal $m15-diffuse {0.5}
	    setGlobal $m15-specular {0.388}
	    setGlobal $m15-shine {24}
            setGlobal $m15-adaptive {1}
	    global $m15-shading-button-state
	    trace variable $m15-shading-button-state w \
		"$this update_BioImage_shading_button_state"

	    setGlobal $m19-bins {3000}
	    setGlobal $m19-sbins {1}

	    setGlobal $m20-gamma {0.5}

	    setGlobal $m21-nbits {8}
	    setGlobal $m21-useinputmin 1
	    setGlobal $m21-useinputmax 1

	    setGlobal $m22-bins {512 256}
	    setGlobal $m22-mins {nan nan}
	    setGlobal $m22-maxs {nan nan}
	    setGlobal $m22-type {nrrdTypeFloat}

	    setGlobal $m23-operator {+}

	    setGlobal $m24-operator {log}

            global $m26-crop_minAxis0 $m26-crop_maxAxis0
            global $m26-crop_minAxis1 $m26-crop_maxAxis1
            global $m26-crop_minAxis2 $m26-crop_maxAxis2
	    trace variable $m26-crop_minAxis0 w "$this update_crop_values"
	    trace variable $m26-crop_minAxis1 w "$this update_crop_values"
	    trace variable $m26-crop_minAxis2 w "$this update_crop_values"
	    trace variable $m26-crop_maxAxis0 w "$this update_crop_values"
	    trace variable $m26-crop_maxAxis1 w "$this update_crop_values"
	    trace variable $m26-crop_maxAxis2 w "$this update_crop_values"
	    trace variable $m26-geom_flushed  w "$this maybe_autoview"

	    global planes_mapType
	    setGlobal $m27-mapType $planes_mapType
	    setGlobal $m27-width 441
	    setGlobal $m24-height 40
	    
	    # intialize at full alpha
	    setGlobal $m27-positionList {{0 0} {441 0}}
	    setGlobal $m27-nodeList {514 1055}

	    setGlobal $m30-axis 0

	    setGlobal $m31-axis 1

	    setGlobal $m32-axis 2

	    setGlobal $m37-port-index 1

	    set mod_list [list $m1 $m2 $m3 $m4 $m5 $m6 $m7 $m8 $m9 $m10 $m11 $m12 $m13 $m14 $m15 $m16 $m17 $m18 $m19 $m20 $m21 $m22 $m23 $m24 $m25 $m26 $m27 $m28 $m29 $m30 $m31 $m32 $m33 $m34 $m35 $m36 $m37]
	    set filters(0) [list load $mod_list [list $m6] [list $m35 0] start end 0 0 1 "Data - Unknown"]

            $this build_viewers $m25 $m26
	}
	
	set f [add_Load_UI $history 0 0]

        # Add insert bar
        $this add_insert_bar $f 0
    }
    
    method add_Load_UI {history row which} {
	global mods

	frame $history.$which
	grid config $history.$which -column 0 -row $row -sticky "nw" -pady 0

	### Load Data UI
	set ChooseNrrd [lindex [lindex $filters($which) $modules] $load_choose_vis] 
	global eye
 	radiobutton $history.$which.eye$which -text "" \
 	    -variable eye -value $which \
	    -command "$this change_eye $which"
	Tooltip $history.$which.eye$which "Select to change current view\nof 3D and 2D windows"
	
 	grid config $history.$which.eye$which -column 0 -row 0 -sticky "nw"
	
 	iwidgets::labeledframe $history.$which.f$which \
 	    -labeltext "Load Data" \
 	    -labelpos nw 

 	grid config $history.$which.f$which -column 1 -row 0 -sticky "nw"

 	set data [$history.$which.f$which childsite]
	
 	frame $data.expand 
 	pack $data.expand -side top -anchor nw
	
 	set image_dir [netedit getenv SCIRUN_SRCDIR]/pixmaps
 	set show [image create photo -file ${image_dir}/play-icon-small.ppm]
 	button $data.expand.b -image $show \
 	    -anchor nw \
 	    -command "$this change_visibility $which" \
 	    -relief flat
	Tooltip $data.expand.b "Click to minimize/show\nthe Load UI"
 	label $data.expand.l -text "Data - Unknown" -width [expr $label_width+2] \
 	    -anchor nw
	Tooltip $data.expand.l "Right click to edit label."

 	pack $data.expand.b $data.expand.l -side left -anchor nw 
	
 	bind $data.expand.l <ButtonPress-3> "$this change_label %X %Y $which"
	
 	frame $data.ui
 	pack $data.ui -side top -anchor nw -expand yes -fill x

 	label $data.ui.samples -text "Original Samples: unknown"
 	pack $data.ui.samples -side top -anchor nw -pady 3

	# Build data tabs
	iwidgets::tabnotebook $data.ui.tnb \
	    -width [expr $process_width - 110] -height 75 \
	    -tabpos n -equaltabs false
	pack $data.ui.tnb -side top -anchor nw \
	    -padx 0 -pady 3
	Tooltip $data.ui.tnb "Load 3D volume in Nrrd,\nDicom, Analyze, or Field format."
	
	# Make pointers to modules 
	set NrrdReader  [lindex [lindex $filters($which) $modules] $load_nrrd]
	set DicomNrrdReader  [lindex [lindex $filters($which) $modules] $load_dicom]
	set AnalzyeNrrdReader  [lindex [lindex $filters($which) $modules] $load_analyze]
	set FieldReader  [lindex [lindex $filters($which) $modules] $load_field]

	# Nrrd
	set page [$data.ui.tnb add -label "Nrrd" \
		      -command "$this set_cur_data_tab Nrrd; $this configure_readers Nrrd"]       

	global [set NrrdReader]-filename
	frame $page.file
	pack $page.file -side top -anchor nw -padx 3 -pady 0 -fill x

	label $page.file.l -text "Nrrd File:" 
	entry $page.file.e -textvariable [set NrrdReader]-filename 
	Tooltip $page.file.e "Currently loaded data set"
	pack $page.file.l $page.file.e -side left -padx 3 -pady 0 -anchor nw \
	    -fill x 

	bind $page.file.e <Return> "$this update_changes"
	bind $page.file.e <ButtonPress-1> "$this check_crop"

	trace variable [set NrrdReader]-filename w "$this enable_update"
	
	button $page.load -text "Browse" \
	    -command "$this check_crop; $this open_nrrd_reader_ui $which" \
	    -width 12
	Tooltip $page.load "Use a file browser to\nselect a Nrrd data set"
	pack $page.load -side top -anchor n -padx 3 -pady 1
	
	
	### Dicom
	set page [$data.ui.tnb add -label "Dicom" \
		      -command "$this set_cur_data_tab Dicom; $this configure_readers Dicom"]
	
	button $page.load -text "Dicom Loader" \
	    -command "$this check_crop; $this enable_update 1 2 3; $this dicom_ui"
	Tooltip $page.load "Open Dicom Load user interface"
	
	pack $page.load -side top -anchor n \
	    -padx 3 -pady 10 -ipadx 2 -ipady 2
	
	### Analyze
	set page [$data.ui.tnb add -label "Analyze" \
		      -command "$this set_cur_data_tab Analyze; $this configure_readers Analyze"]
	
	button $page.load -text "Analyze Loader" \
	    -command "$this check_crop; $$this enable_update 1 2 3; this analyze_ui"
	Tooltip $page.load "Open Dicom Load user interface"
	
	pack $page.load -side top -anchor n \
	    -padx 3 -pady 10 -ipadx 2 -ipady 2
	
	### Field
	set page [$data.ui.tnb add -label "Field" \
		      -command "$this configure_readers Field"]
	
	global [set FieldReader]-filename
	frame $page.file
	pack $page.file -side top -anchor nw -padx 3 -pady 0 -fill x

	label $page.file.l -text "Field File:" 
	entry $page.file.e -textvariable [set FieldReader]-filename 
	pack $page.file.l $page.file.e -side left -padx 3 -pady 0 -anchor nw \
	    -fill x 

	bind $page.file.e <Return> "$this update_changes"
	bind $page.file.e <ButtonPress-1> "$this check_crop"

	button $page.load -text "Browse" \
	    -command "$this open_field_reader_ui $which" \
	    -width 12
	Tooltip $page.load "Use a file browser to\nselect a Nrrd data set"

        trace variable [set FieldReader]-filename w "$this enable_update"
	pack $page.load -side top -anchor n -padx 3 -pady 1
	
	# Set default view to be Nrrd
	$data.ui.tnb view "Nrrd"

	frame $data.ui.f
	pack $data.ui.f
	
	set w $data.ui.f
	
	# Orientations button
	set image_dir  [netedit getenv SCIRUN_SRCDIR]/pixmaps
	set show [image create photo -file ${image_dir}/OrientationsCube.ppm]
	button $w.orient -image $show \
	    -anchor nw \
	    -command "$this check_crop; $this update_orientations"
	Tooltip $w.orient "Edit the entries to indicate the various orientations.\nOptions include Superior (S) or Inferior (I),\nAnterior (A) or Posterior (P), and Left (L) or Right (R).\nTo update the orientations, press the cube image."
	grid config $w.orient -row 0 -column 1 -columnspan 3 -rowspan 4 -sticky "n"


	# Top entry
	global top
	entry $w.tentry -textvariable top -width 3
	Tooltip $w.tentry "Indicates the current orientation.\nOptions include Superior (S) or Inferior (I),\nAnterior (A) or Posterior (P), and Left (L) or Right (R).\nTo update the orientations, press the cube image."
	grid config $w.tentry -row 0 -column 0 -sticky "e"

        bind $w.tentry <ButtonPress-1> "$this check_crop"

	# Front entry
	global front
	entry $w.fentry -textvariable front -width 3
	Tooltip $w.fentry "Indicate the current orientation.\nOptions include Superior (S) or Inferior (I),\nAnterior (A) or Posterior (P), and Left (L) or Right (R).\nTo update the orientations, press the cube image."
	grid config $w.fentry -row 4 -column 2 -sticky "nw"

        bind $w.fentry <ButtonPress-1> "$this check_crop"
	
	
	# Side entry
	global side
	entry $w.sentry -textvariable side -width 3
	Tooltip $w.sentry "Indicates the current orientations.\nOptions include Superior (S) or Inferior (I),\nAnterior (A) or Posterior (P), and Left (L) or Right (R).\nTo update the orientations, press the cube image."
	grid config $w.sentry -row 1 -column 4 -sticky "n"

        bind $w.sentry <ButtonPress-1> "$this check_crop"

        trace variable top w "$this enable_update"
        trace variable front w "$this enable_update"
        trace variable side w "$this enable_update"

        # reset button
	button $data.ui.reset -text "Reset" -command "$this check_crop; $this reset_orientations"
	Tooltip $data.ui.reset "Reset the orientation labels to defaults."
	pack $data.ui.reset -side right -anchor se -padx 4 -pady 4

	return $history.$which
    }

    method open_nrrd_reader_ui {i} {
	# disable execute button and change behavior of execute command
	set m [lindex [lindex $filters($i) $modules] 0]

	[set m] initialize_ui

	.ui[set m].f7.execute configure -state disabled

	upvar #0 .ui[set m] data	
	set data(-command) "wm withdraw .ui[set m]"
    }

    method open_field_reader_ui {i} {
	# disable execute button and change behavior of execute command
	set m [lindex [lindex $filters($i) $modules] 3]

	[set m] initialize_ui

	.ui[set m].f7.execute configure -state disabled

	upvar #0 .ui[set m] data
	set data(-command) "wm withdraw .ui[set m]"
    }

    method dicom_ui { } {
	set m [lindex [lindex $filters(0) $modules] 1]
	$m initialize_ui

	if {[winfo exists .ui$m]} {
	    # disable execute button 
	    .ui$m.buttonPanel.btnBox.execute configure -state disabled
	}

	global $m-dir $m-num-files
	trace variable $m-dir w "$this enable_update"
	trace variable $m-num-files w "$this enable_update"
    }

    method analyze_ui { } {
	set m [lindex [lindex $filters(0) $modules] 2]
	$m initialize_ui
	if {[winfo exists .ui$m]} {
	    # disable execute button 
	    .ui$m.buttonPanel.btnBox.execute configure -state disabled
	}
	global $m-file $m-num-files
	trace variable $m-file w "$this enable_update"
	trace variable $m-num-files w "$this enable_update"
    }


    ### update/reset_orientations
    #################################################
    method reset_orientations {} {
	global top front side

        # disable flip and permute modules and change choose ports
	set UnuFlip1 [lindex [lindex $filters(0) $modules] 29]
	set Choose1 [lindex [lindex $filters(0) $modules] 32]
	global [set Choose1]-port-index
        disableModule [set UnuFlip1] 1
	set [set Choose1]-port-index 0

	set UnuFlip2 [lindex [lindex $filters(0) $modules] 30]
	set Choose2 [lindex [lindex $filters(0) $modules] 33]
	global [set Choose2]-port-index
        disableModule [set UnuFlip2] 1
	set [set Choose2]-port-index 0

	set UnuFlip3 [lindex [lindex $filters(0) $modules] 31]
	set Choose3 [lindex [lindex $filters(0) $modules] 34]
	global [set Choose3]-port-index
        disableModule [set UnuFlip3] 1
	set [set Choose3]-port-index 0

	set UnuPermute [lindex [lindex $filters(0) $modules] 28]
	set Choose4 [lindex [lindex $filters(0) $modules] 35]
	global [set Choose4]-port-index
        disableModule [set UnuPermute] 1
	set [set Choose4]-port-index 0

	set top "S"
	set front "A"
	set side "L"

	# Re-execute
	if {$has_executed} {
	    set m [lindex [lindex $filters(0) $modules] 5]
	    $m-c needexecute
	}
    }
    method update_orientations {} {
	global top front side

	# check that they are all different
	if {[string eq $top $front] || [string eq $top $side] || \
		[string eq $front $side]} {
	    tk_messageBox -message "Orientations must all be different\nand consist of a Superior (S)\nor Inferior (I), an Anterior (A)\nor a Posterior (P), and a Left (L)\nor Right (R)." -type ok -icon info -parent .standalone
	    return
	}

	# check that each is S/s/I/i A/a/P/p L/l/R/r
 	if {$front != "S" && $front != "s" && \
		$front != "I" && $front != "i" && \
		$front != "A" && $front != "a" && \
		$front != "P" && $front != "p" && \
		$front != "L" &&  $front != "l" && \
		$front != "R" && $front != "r"} {
	    tk_messageBox -message "Orientations must all be different\nand consist of a Superior (S)\nor Inferior (I), an Anterior (A)\nor a Posterior (P), and a Left (L)\nor Right (R)." -type ok -icon info -parent .standalone
	    return
	}

	if {$top != "S" && $top != "s" && $top != "I" && \
		$top != "i" && $top != "A" && $top != "a" && \
		$top != "P" && $top != "p" && $top != "L" && \
		$top != "l" && $top != "R" && $top != "r"} {
	    tk_messageBox -message "Orientations must all be different\nand consist of a Superior (S)\nor Inferior (I), an Anterior (A)\nor a Posterior (P), and a Left (L)\nor Right (R)." -type ok -icon info -parent .standalone
	    return
	} 
	
	if {$side != "S" && $side != "s" && $side != "I" &&  \
		$side != "i" && $side != "A" && $side != "a" && \
		$side != "P" && $side != "p" && $side != "L" && \
		$side != "l" && $side != "R" &&  $side != "r"} {
	    tk_messageBox -message "Orientations must all be different\nand consist of a Superior (S)\nor Inferior (I), an Anterior (A)\nor a Posterior (P), and a Left (L)\nor Right (R)." -type ok -icon info -parent .standalone
	    return
	} 

	# reset any downstream crop and resample params and issue
	# warning to user
	set reset 0
	for {set i 1} {$i < $num_filters} {incr i} {
	    if {[lindex $filters($i) $filter_type] == "crop" &&
		[lindex $filters($i) $which_row] != -1} {
		set reset 1
	    } elseif  {[lindex $filters($i) $filter_type] == "resample" &&
		      [lindex $filters($i) $which_row] != -1} {
		set reset 1
	    }
	}

	if {$reset == 1} {
	    set result [tk_messageBox -message "Downstream crop and resample filters will be reset. Do you want to proceed with changing the orientation?" -type okcancel -icon info -parent .standalone]
	    if {$result == "cancel"} {
		return
	    }
	}
		
	for {set i 1} {$i < $num_filters} {incr i} {
	    if {[lindex $filters($i) $filter_type] == "crop" &&
		[lindex $filters($i) $which_row] != -1} {
		set reset 1
		set UnuCrop [lindex [lindex $filters($i) $modules] 0]
		global [set UnuCrop]-minAxis0
		global [set UnuCrop]-maxAxis0
		global [set UnuCrop]-minAxis1
		global [set UnuCrop]-maxAxis1
		global [set UnuCrop]-minAxis2
		global [set UnuCrop]-maxAxis2
		set [set UnuCrop]-minAxis0 0
		set [set UnuCrop]-maxAxis0 M
		set [set UnuCrop]-minAxis1 0
		set [set UnuCrop]-maxAxis1 M
		set [set UnuCrop]-minAxis2 0
		set [set UnuCrop]-maxAxis2 M
	    } elseif {[lindex $filters($i) $filter_type] == "resample" &&
		      [lindex $filters($i) $which_row] != -1} {
		set reset 1
		set UnuResample [lindex [lindex $filters($i) $modules] 0]
		global [set UnuResample]-resampAxis0
		global [set UnuResample]-resampAxis1
		global [set UnuResample]-resampAxis2
		set [set UnuResample]-resampAxis0 "x1"
		set [set UnuResample]-resampAxis1 "x1"
		set [set UnuResample]-resampAxis2 "x1"
	    }
	}

	# Permute into order where top   = S/I
	#                          front = A/P
	#                          side    L/R
	set new_side 0
	set new_front 1
	set new_top 2

	set c_side "L"
	set c_front "A"
	set c_top "I"

	
	set need_permute 0
	# Check side variable which corresponds to axis 0
	if {$side == "L" || $side == "l"} {
	    set new_side 0
	    set c_side "L"
	} elseif {$side == "R" || $side == "r"} {
	    set new_side 0
	    set c_side "R"
	} elseif {$side == "A" || $side == "a"} {
	    set new_side 1
	    set need_permute 1
	    set c_side "A"
	} elseif {$side == "P" || $side == "p"} {
	    set new_side 1
	    set need_permute 1
	    set c_side "P"
	} elseif {$side == "S" || $side == "s"} {
	    set new_side 2
	    set need_permute 1
	    set c_side "S"
	} else {
	    set new_side 2
	    set need_permute 1
	    set c_side "I"
	}

	# Check front variable which corresponds to axis 1
	if {$front == "A" || $front == "a"} {
	    set new_front 1
	    set c_front "A"
	} elseif {$front == "P" || $front == "p"} {
	    set new_front 1
	    set c_front "P"
	} elseif {$front == "L" || $front == "l"} {
	    set new_front 0
	    set need_permute 1
	    set c_front "L"
	} elseif {$front == "R" || $front == "r"} {
	    set new_front 0
	    set need_permute 1
	    set c_front "R"
	} elseif {$front == "S" || $front == "s"} {
	    set new_front 2
	    set need_permute 1
	    set c_front "S"
	} else {
	    set new_front 2
	    set need_permute 1
	    set c_front "I"
	}

	# Check top variable which is axis 2
	if {$top == "S" || $top == "s"} {
	    set new_top 2
	    set c_top "S"
	} elseif {$top == "I" || $top == "i"} {
	    set new_top 2
	    set c_top "I"
	} elseif {$top == "L" || $top == "l"} { 
	    set new_top 0
	    set need_permute 1
	    set c_top "L"
	} elseif {$top == "R" || $top == "r"} {
	    set new_top 0
	    set need_permute 1
	    set c_top "R"
	} elseif {$top == "A" || $top == "a"} {
	    set new_top 1
	    set need_permute 1
	    set c_top "A"
	} else {
	    set new_top 1
	    set need_permute 1
	    set c_top "I"
	}

	# only use permute if needed to avoid copying data
	set UnuPermute [lindex [lindex $filters(0) $modules] 28]
	set Choose [lindex [lindex $filters(0) $modules] 35]
	global [set Choose]-port-index

	if {$need_permute == 1} {
	    set [set Choose]-port-index 1
	    disableModule [set UnuPermute] 0
	    global [set UnuPermute]-axis0 [set UnuPermute]-axis1 [set UnuPermute]-axis2

	    set [set UnuPermute]-axis0 $new_side
	    set [set UnuPermute]-axis1 $new_front
	    set [set UnuPermute]-axis2 $new_top
	} else {
	    set [set Choose]-port-index 0
	    disableModule [set UnuPermute] 1
	}

	set flip_0 0
	set flip_1 0
	set flip_2 0

	# only flip axes if needed
	if {$c_side != "L"} {
	    # need to flip axis 0
	    $this flip0 1
	    set flip_0 1
	} else {
	    $this flip0 0
	}

	if {$c_front != "A"} {
	    # need to flip axis 1
	    $this flip1 1
	    set flip_1 1
	} else {
	    $this flip1 0
	}

	if {$c_top != "S"} {
	    # need to flip axis 2
	    $this flip2 1
	    set flip_2 1
	} else {
	    $this flip2 0
	}

	# Re-execute
	if {!$loading && $has_executed} {
	    if {$need_permute == 1} {
		[set UnuPermute]-c needexecute
	    } elseif {$flip_0 == 1} {
		set UnuFlip [lindex [lindex $filters(0) $modules] 29]
		[set UnuFlip]-c needexecute
	    } elseif {$flip_1 == 1} {
		set UnuFlip [lindex [lindex $filters(0) $modules] 30]
		[set UnuFlip]-c needexecute
	    } elseif {$flip_2 == 1} {
		set UnuFlip [lindex [lindex $filters(0) $modules] 31]
		[set UnuFlip]-c needexecute
	    } else {
		set m [lindex [lindex $filters(0) $modules] 5]
		$m-c needexecute
	    }
	}
    }

    method flip0 { toflip } {
	set UnuFlip [lindex [lindex $filters(0) $modules] 29]
	set Choose [lindex [lindex $filters(0) $modules] 32]
	global [set Choose]-port-index
	
	if {$toflip == 1} {
	    disableModule [set UnuFlip] 0
	    set [set Choose]-port-index 1
	} else {
	    disableModule [set UnuFlip] 1
	    set [set Choose]-port-index 0
	}
    }
    
    method flip1 { toflip } {
	set UnuFlip [lindex [lindex $filters(0) $modules] 30]
	set Choose [lindex [lindex $filters(0) $modules] 33]
	global [set Choose]-port-index
	
	if {$toflip == 1} {
	    disableModule [set UnuFlip] 0
	    set [set Choose]-port-index 1
	} else {
	    disableModule [set UnuFlip] 1
	    set [set Choose]-port-index 0
	}
    }
    
    method flip2 { toflip } {
	set UnuFlip [lindex [lindex $filters(0) $modules] 31]
	set Choose [lindex [lindex $filters(0) $modules] 34]
	global [set Choose]-port-index
	
	if {$toflip == 1} {
	    disableModule [set UnuFlip] 0
	    set [set Choose]-port-index 1
	} else {
	    disableModule [set UnuFlip] 1
	    set [set Choose]-port-index 0
	}
    }
    
    ##############################
    ### configure_readers
    ##############################
    # Keeps the readers in sync.  Every time a different
    # data tab is selected (Nrrd, Dicom, Analyze) the other
    # readers must be disabled to avoid errors.
    method configure_readers { which } {

        $this check_crop

	set ChooseNrrd  [lindex [lindex $filters(0) $modules] $load_choose_input]
	set NrrdReader  [lindex [lindex $filters(0) $modules] $load_nrrd]
	set DicomNrrdReader  [lindex [lindex $filters(0) $modules] $load_dicom]
	set AnalyzeNrrdReader  [lindex [lindex $filters(0) $modules] $load_analyze]
	set FieldReader  [lindex [lindex $filters(0) $modules] $load_field]

        global [set ChooseNrrd]-port-index

	if {$which == "Nrrd"} {
	    set [set ChooseNrrd]-port-index 0
	    disableModule $NrrdReader 0
	    disableModule $DicomNrrdReader 1
	    disableModule $AnalyzeNrrdReader 1
	    disableModule $FieldReader 1
# 	    if {$initialized != 0} {
# 		$data_tab0 view "Nrrd"
# 		$data_tab1 view "Nrrd"
# 		set c_data_tab "Nrrd"
# 	    }
        } elseif {$which == "Dicom"} {
	    set [set ChooseNrrd]-port-index 1
	    disableModule $NrrdReader 1
	    disableModule $DicomNrrdReader 0
	    disableModule $AnalyzeNrrdReader 1
	    disableModule $FieldReader 1
#             if {$initialized != 0} {
# 		$data_tab0 view "Dicom"
# 		$data_tab1 view "Dicom"
# 		set c_data_tab "Dicom"
# 	    }
        } elseif {$which == "Analyze"} {
	    # Analyze
	    set [set ChooseNrrd]-port-index 2
	    disableModule $NrrdReader 1
	    disableModule $DicomNrrdReader 1
	    disableModule $AnalyzeNrrdReader 0
	    disableModule $FieldReader 1
# 	    if {$initialized != 0} {
# 		$data_tab0 view "Analyze"
# 		$data_tab1 view "Analyze"
# 		set c_data_tab "Analyze"
# 	    }
        } elseif {$which == "Field"} {
	    # Field
	    set [set ChooseNrrd]-port-index 3
	    disableModule $NrrdReader 1
	    disableModule $DicomNrrdReader 1
	    disableModule $AnalyzeNrrdReader 1
	    disableModule $FieldReader 0
# 	    if {$initialized != 0} {
# 		$data_tab0 view "Field"
# 		$data_tab1 view "Field"
# 		set c_data_tab "Field"
# 	    }
	} elseif {$which == "all"} {
	    if {[set [set ChooseNrrd]-port-index] == 0} {
		# nrrd
		disableModule $NrrdReader 0
		disableModule $DicomNrrdReader 1
		disableModule $AnalyzeNrrdReader 1
		disableModule $FieldReader 1
	    } elseif {[set [set ChooseNrrd]-port-index] == 1} {
		# dicom
		disableModule $NrrdReader) 1
		disableModule $DicomNrrdReader) 0
		disableModule $AnalyzeNrrdReader) 1
		disableModule $FieldReader) 1
	    } elseif {[set [set ChooseNrrd]-port-index] == 2} {
		# analyze
		disableModule $NrrdReader 1
		disableModule $DicomNrrdReader 1
		disableModule $AnalyzeNrrdReader 0
		disableModule $FieldReader 1
	    } else {
		# field
		disableModule $NrrdReader 1
		disableModule $DicomNrrdReader 1
		disableModule $AnalyzeNrrdReader 1
		disableModule $FieldReader 0
	    }
	}
    }
    
    

    #############################
    ### init_Vframe
    #############################
    # Initialize the visualization frame on the right. For this app
    # that includes the Planes, Volume Rendering, and 3D Options tabs.  
    method init_Vframe { m case} {
	global mods
	global tips
	if { [winfo exists $m] } {
	    ### Visualization Frame
	    iwidgets::labeledframe $m.vis \
		-labelpos n -labeltext "Visualization Settings" 
	    pack $m.vis -side right -anchor n -fill y
	    
	    set vis [$m.vis childsite]
	    
	    ### Tabs
	    iwidgets::tabnotebook $vis.tnb -width $notebook_width \
		-height [expr $vis_height - 25] -tabpos n \
                -equaltabs false
	    pack $vis.tnb -padx 0 -pady 0 -anchor n -fill both -expand 1

            set vis_frame_tab$case $vis.tnb


	    set page [$vis.tnb add -label "Planes" -command "$this change_vis_frame Planes; $this check_crop"]

            frame $page.planes 
            pack $page.planes -side top -anchor nw -expand no -fill x

	    global show_plane_x show_plane_y show_plane_z
	    global show_MIP_x show_MIP_y show_MIP_z

	    checkbutton $page.planes.xp -text "Show Sagittal Plane" \
		-variable show_plane_x \
		-command "$this toggle_show_plane_x"
            Tooltip $page.planes.xp "Turn Sagittal plane on/off"

  	    checkbutton $page.planes.xm -text "Show Sagittal MIP" \
  		-variable show_MIP_x \
  		-command "$this toggle_show_MIP_x"
            Tooltip $page.planes.xm "Turn Sagittal MIP on/off"


	    checkbutton $page.planes.yp -text "Show Coronal Plane" \
		-variable show_plane_y \
		-command "$this toggle_show_plane_y"
            Tooltip $page.planes.yp "Turn Coronal plane on/off"

  	    checkbutton $page.planes.ym -text "Show Coronal MIP" \
  		-variable show_MIP_y \
  		-command "$this toggle_show_MIP_y"
              Tooltip $page.planes.ym "Turn Coronal MIP on/off"


	    checkbutton $page.planes.zp -text "Show Axial Plane" \
		-variable show_plane_z \
		-command "$this toggle_show_plane_z"
            Tooltip $page.planes.zp "Turn Axial plane on/off"

  	    checkbutton $page.planes.zm -text "Show Axial MIP" \
  		-variable show_MIP_z \
  		-command "$this toggle_show_MIP_z"
              Tooltip $page.planes.zm "Turn Axial MIP on/off"


            grid configure $page.planes.xp -row 0 -column 0 -sticky "w"
            grid configure $page.planes.xm -row 0 -column 1 -sticky "w"
            grid configure $page.planes.yp -row 1 -column 0 -sticky "w"
            grid configure $page.planes.ym -row 1 -column 1 -sticky "w"
            grid configure $page.planes.zp -row 2 -column 0 -sticky "w"
            grid configure $page.planes.zm -row 2 -column 1 -sticky "w"

            # display window and level
            global $mods(ViewSlices)-axial-viewport0-clut_ww 
            global $mods(ViewSlices)-axial-viewport0-clut_wl
            global $mods(ViewSlices)-min $mods(ViewSlices)-max

            iwidgets::labeledframe $page.winlevel \
                 -labeltext "Window/Level Controls" \
                 -labelpos nw
            pack $page.winlevel -side top -anchor nw -expand no -fill x \
                -pady 3
            set winlevel [$page.winlevel childsite]

            frame $winlevel.ww
            frame $winlevel.wl
            pack $winlevel.ww $winlevel.wl -side top -anchor ne -pady 0

            label $winlevel.ww.l -text "Window Width"
            scale $winlevel.ww.s -variable $mods(ViewSlices)-axial-viewport0-clut_ww \
                -from 0 -to 9999 -length 130 -width 15 \
                -showvalue false -orient horizontal \
                -command "$this change_window_width"
            Tooltip $winlevel.ww.s "Control the window width of\nthe 2D viewers"
            bind $winlevel.ww.s <ButtonRelease> "$this execute_vol_ren_when_linked"
            entry $winlevel.ww.e -textvariable $mods(ViewSlices)-axial-viewport0-clut_ww 
            bind $winlevel.ww.e <Return> "$this change_window_width 1; $this execute_vol_ren_when_linked"
            pack $winlevel.ww.l $winlevel.ww.s $winlevel.ww.e -side left

            label $winlevel.wl.l -text "Window Level "
            scale $winlevel.wl.s -variable $mods(ViewSlices)-axial-viewport0-clut_wl \
                -from 0 -to 9999 -length 130 -width 15 \
                -showvalue false -orient horizontal \
                -command "$this change_window_level"
            Tooltip $winlevel.wl.s "Control the window level of\nthe 2D viewers"
            bind $winlevel.wl.s <ButtonRelease> "$this execute_vol_ren_when_linked"
            entry $winlevel.wl.e -textvariable $mods(ViewSlices)-axial-viewport0-clut_wl 
            bind $winlevel.wl.e <Return> "$this change_window_level 1; $this execute_vol_ren_when_linked"
            pack $winlevel.wl.l $winlevel.wl.s $winlevel.wl.e -side left

            trace variable $mods(ViewSlices)-min w "$this update_window_level_scales"
            trace variable $mods(ViewSlices)-max w "$this update_window_level_scales"
            # Background threshold
            frame $page.thresh 
            pack $page.thresh -side top -anchor nw -expand no -fill x

            label $page.thresh.l -text "Background Threshold:"

            scale $page.thresh.s \
                -from 0 -to 100 \
 	        -orient horizontal -showvalue false \
 	        -length 100 -width 15 \
	        -variable $mods(ViewSlices)-background_threshold \
                -command "$mods(ViewSlices)-c background_thresh"
	    bind $page.thresh.s <Button1-Motion> "$mods(ViewSlices)-c background_thresh"
            bind $page.thresh.s <ButtonPress-1> "$this check_crop"
            label $page.thresh.l2 -textvariable $mods(ViewSlices)-background_threshold
            Tooltip $page.thresh.s "Clip out values less than\nspecified background threshold"

            pack $page.thresh.l $page.thresh.s $page.thresh.l2 -side left -anchor nw \
                -padx 2 -pady 2

            Tooltip $page.thresh.l "Change background threshold. Data\nvalues less than or equal to the threshold\nwill be transparent in planes."
            Tooltip $page.thresh.s "Change background threshold. Data\nvalues less than or equal to the threshold\nwill be transparent in planes."
            Tooltip $page.thresh.l2 "Change background threshold. Data\nvalues less than or equal to the threshold\nwill be transparent in planes."


	    checkbutton $page.lines -text "Show Guidelines" \
		-variable show_guidelines \
		-command "$this toggle_show_guidelines" 
            pack $page.lines -side top -anchor nw -padx 4 -pady 7
            Tooltip $page.lines "Toggle 2D Viewer guidelines"

            global filter2Dtextures
	    checkbutton $page.2Dtext -text "Filter 2D Textures" \
		-variable filter2Dtextures \
		-command "$this toggle_filter2Dtextures" 
            pack $page.2Dtext -side top -anchor nw -padx 4 -pady 7
            Tooltip $page.2Dtext "Turn filtering 2D textures\non/off"

	    global planes_color
	    iwidgets::labeledframe $page.isocolor \
		-labeltext "Color Planes By" \
		-labelpos nw 
	    pack $page.isocolor -side top -anchor n -padx 3 -pady 0 -fill x
	    
	    set maps [$page.isocolor childsite]

	    global planes_mapType
	    
	    # Gray
	    frame $maps.gray
	    pack $maps.gray -side top -anchor nw -padx 3 -pady 1 \
		-fill x -expand 1
	    radiobutton $maps.gray.b -text "Gray" \
		-variable planes_mapType \
		-value 0 \
		-command "$this update_planes_color_by"
            Tooltip $maps.gray.b "Select color map for coloring planes"
	    pack $maps.gray.b -side left -anchor nw -padx 3 -pady 0
	    
	    frame $maps.gray.f -relief sunken -borderwidth 2
	    pack $maps.gray.f -padx 2 -pady 0 -side right -anchor e
	    canvas $maps.gray.f.canvas -bg "#ffffff" -height $colormap_height -width $colormap_width
	    pack $maps.gray.f.canvas -anchor e \
		-fill both -expand 1
	    
	    draw_colormap Gray $maps.gray.f.canvas
	    
	    # Rainbow
	    frame $maps.rainbow
	    pack $maps.rainbow -side top -anchor nw -padx 3 -pady 1 \
		-fill x -expand 1
	    radiobutton $maps.rainbow.b -text "Rainbow" \
		-variable planes_mapType \
		-value 3 \
		-command "$this update_planes_color_by"
            Tooltip $maps.rainbow.b "Select color map for coloring planes"
	    pack $maps.rainbow.b -side left -anchor nw -padx 3 -pady 0
	    
	    frame $maps.rainbow.f -relief sunken -borderwidth 2
	    pack $maps.rainbow.f -padx 2 -pady 0 -side right -anchor e
	    canvas $maps.rainbow.f.canvas -bg "#ffffff" -height $colormap_height -width $colormap_width
	    pack $maps.rainbow.f.canvas -anchor e
	    
	    draw_colormap Rainbow $maps.rainbow.f.canvas
	    
	    # Darkhue
	    frame $maps.darkhue
	    pack $maps.darkhue -side top -anchor nw -padx 3 -pady 1 \
		-fill x -expand 1
	    radiobutton $maps.darkhue.b -text "Darkhue" \
		-variable planes_mapType \
		-value 4 \
		-command "$this update_planes_color_by"
            Tooltip $maps.darkhue.b "Select color map for coloring planes"
	    pack $maps.darkhue.b -side left -anchor nw -padx 3 -pady 0
	    
	    frame $maps.darkhue.f -relief sunken -borderwidth 2
	    pack $maps.darkhue.f -padx 2 -pady 0 -side right -anchor e
	    canvas $maps.darkhue.f.canvas -bg "#ffffff" -height $colormap_height -width $colormap_width
	    pack $maps.darkhue.f.canvas -anchor e
	    
	    draw_colormap Darkhue $maps.darkhue.f.canvas
	    
	    
	    # Blackbody
	    frame $maps.blackbody
	    pack $maps.blackbody -side top -anchor nw -padx 3 -pady 1 \
		-fill x -expand 1
	    radiobutton $maps.blackbody.b -text "Blackbody" \
		-variable planes_mapType \
		-value 7 \
		-command "$this update_planes_color_by"
            Tooltip $maps.blackbody.b "Select color map for coloring planes"
	    pack $maps.blackbody.b -side left -anchor nw -padx 3 -pady 0
	    
	    frame $maps.blackbody.f -relief sunken -borderwidth 2 
	    pack $maps.blackbody.f -padx 2 -pady 0 -side right -anchor e
	    canvas $maps.blackbody.f.canvas -bg "#ffffff" -height $colormap_height -width $colormap_width
	    pack $maps.blackbody.f.canvas -anchor e
	    
	    draw_colormap Blackbody $maps.blackbody.f.canvas
	    
	    # Blue-to-Red
	    frame $maps.bpseismic
	    pack $maps.bpseismic -side top -anchor nw -padx 3 -pady 1 \
		-fill x -expand 1
	    radiobutton $maps.bpseismic.b -text "Blue-to-Red" \
		-variable planes_mapType \
		-value 17 \
		-command "$this update_planes_color_by"
            Tooltip $maps.bpseismic.b "Select color map for coloring planes"
	    pack $maps.bpseismic.b -side left -anchor nw -padx 3 -pady 0
	    
	    frame $maps.bpseismic.f -relief sunken -borderwidth 2
	    pack $maps.bpseismic.f -padx 2 -pady 0 -side right -anchor e
	    canvas $maps.bpseismic.f.canvas -bg "#ffffff" -height $colormap_height -width $colormap_width
	    pack $maps.bpseismic.f.canvas -anchor e
	    
	    draw_colormap "Blue-to-Red" $maps.bpseismic.f.canvas           


            #######
            set page [$vis.tnb add -label "Volume Rendering" -command "$this change_vis_frame \"Volume Rendering\"; $this check_crop"]

            global show_volume_ren
	    checkbutton $page.toggle -text "Show Volume Rendering" \
		-variable show_vol_ren \
		-command "$this toggle_show_vol_ren"
            Tooltip $page.toggle "Turn volume rendering on/off"
            pack $page.toggle -side top -anchor nw -padx 3 -pady 3


            button $page.vol -text "Edit Transfer Function" \
               -command "$this check_crop; $mods(EditColorMap2D) initialize_ui;
                         wm title .ui${mods(EditColorMap2D)} {Transfer Function Editor}"
            Tooltip $page.vol "Open up the interface\nfor editing the transfer function"
            pack $page.vol -side top -anchor n -padx 3 -pady 3
            
            set VolumeVisualizer [lindex [lindex $filters(0) $modules] 14]
            set n "$this check_crop; [set VolumeVisualizer]-c needexecute"

            global [set VolumeVisualizer]-render_style

            frame $page.fmode
            pack $page.fmode -padx 2 -pady 2 -fill x
            label $page.fmode.mode -text "Mode"
	    radiobutton $page.fmode.modeo -text "Over Operator" -relief flat \
		    -variable [set VolumeVisualizer]-render_style -value 0 \
    	  	    -anchor w -command $n
   	    radiobutton $page.fmode.modem -text "MIP" -relief flat \
		    -variable [set VolumeVisualizer]-render_style -value 1 \
		    -anchor w -command $n
   	    pack $page.fmode.mode $page.fmode.modeo $page.fmode.modem \
                -side left -fill x -padx 4 -pady 4

            frame $page.fres
            pack $page.fres -padx 2 -pady 2 -fill x
            label $page.fres.res -text "Resolution (bits)"
	    radiobutton $page.fres.b0 -text 8 -variable [set VolumeVisualizer]-blend_res -value 8 \
	        -command $n
    	    radiobutton $page.fres.b1 -text 16 -variable [set VolumeVisualizer]-blend_res -value 16 \
	        -command $n
 	    radiobutton $page.fres.b2 -text 32 -variable [set VolumeVisualizer]-blend_res -value 32 \
	        -command $n
	    pack $page.fres.res $page.fres.b0 $page.fres.b1 $page.fres.b2 \
                -side left -fill x -padx 4 -pady 4

        #----------------------------------------------------------
        # Disable Lighting
        #----------------------------------------------------------
        set ChooseNrrdLighting [lindex [lindex $filters(0) $modules] 36]
        global [set ChooseNrrdLighting]-port-index
	checkbutton $page.lighting -text "Compute data for shaded volume rendering" \
            -relief flat \
            -variable [set ChooseNrrdLighting]-port-index -onvalue 1 -offvalue 0 \
            -anchor w -command "$this toggle_compute_shading"
        Tooltip $page.lighting "Turn computing data for shaded volume\nrendering on/off."
        pack $page.lighting -side top -fill x -padx 4

        #-----------------------------------------------------------
        # Shading
        #-----------------------------------------------------------
	checkbutton $page.shading -text "Show shaded volume rendering" -relief flat \
            -variable [set VolumeVisualizer]-shading -onvalue 1 -offvalue 0 \
            -anchor n -command "$n"
        Tooltip $page.shading "If computed, turn use of shading on/off"
        pack $page.shading -side top -fill x -padx 4

        #-----------------------------------------------------------
        # Light
        #-----------------------------------------------------------
 	frame $page.f5
 	pack $page.f5 -padx 2 -pady 2 -fill x
 	label $page.f5.light -text "Attach Light to"
 	radiobutton $page.f5.light0 -text "Light 0" -relief flat \
            -variable [set VolumeVisualizer]-light -value 0 \
            -anchor w -command $n
 	radiobutton $page.f5.light1 -text "Light 1" -relief flat \
            -variable [set VolumeVisualizer]-light -value 1 \
            -anchor w -command $n
        pack $page.f5.light $page.f5.light0 $page.f5.light1 \
            -side left -fill x -padx 4


        #-----------------------------------------------------------
        # Sample Rate
        #-----------------------------------------------------------
        iwidgets::labeledframe $page.samplingrate \
            -labeltext "Sampling Rate" -labelpos nw
        pack $page.samplingrate -side top -anchor nw -expand no -fill x
        set sratehi [$page.samplingrate childsite]

        scale $sratehi.srate_hi \
            -variable [set VolumeVisualizer]-sampling_rate_hi \
            -from 0.5 -to 10.0 \
            -showvalue true -resolution 0.1 \
            -orient horizontal -width 15 
        pack $sratehi.srate_hi -side top -fill x -padx 4
	bind $sratehi.srate_hi <ButtonRelease> $n

        #-----------------------------------------------------------
        # Interactive Sample Rate
        #-----------------------------------------------------------
        iwidgets::labeledframe $page.samplingrate_lo \
            -labeltext "Interactive Sampling Rate" -labelpos nw
        pack $page.samplingrate_lo -side top -anchor nw \
            -expand no -fill x
        set sratelo [$page.samplingrate_lo childsite]

        scale $sratelo.srate_lo \
            -variable [set VolumeVisualizer]-sampling_rate_lo \
            -from 0.1 -to 5.0 \
            -showvalue true -resolution 0.1 \
            -orient horizontal -width 15 
        pack $sratelo.srate_lo -side top -fill x -padx 4
	bind $sratelo.srate_lo <ButtonRelease> $n


        #-----------------------------------------------------------
        # Global Opacity
        #-----------------------------------------------------------
        iwidgets::labeledframe $page.opacityframe \
            -labeltext "Global Opacity" -labelpos nw
        pack $page.opacityframe -side top -anchor nw \
            -expand no -fill x
        set oframe [$page.opacityframe childsite]

	scale $oframe.opacity -variable [set VolumeVisualizer]-alpha_scale \
		-from -1.0 -to 1.0 \
		-showvalue true -resolution 0.001 \
		-orient horizontal -width 15
        pack $oframe.opacity -side top -fill x -padx 4
	bind $oframe.opacity <ButtonRelease> $n




        #-----------------------------------------------------------
        # Volume Rendering Window Level Controls
        #-----------------------------------------------------------
        global vol_width
        global vol_level

        iwidgets::labeledframe $page.winlevel \
            -labeltext "Window/Level Controls" \
            -labelpos nw
        pack $page.winlevel -side top -anchor nw -expand no -fill x
        set winlevel [$page.winlevel childsite]

        global link_winlevel
        checkbutton $winlevel.link -text "Link to Slice Window/Level" \
            -variable link_winlevel \
            -command "$this link_windowlevels"
        Tooltip $winlevel.link "Link the changes of the\nwindow controls below to\nthe planes window controls"
        pack $winlevel.link -side top -anchor nw -pady 1

        frame $winlevel.ww
        frame $winlevel.wl
        pack $winlevel.ww $winlevel.wl -side top -anchor ne -pady 0

        label $winlevel.ww.l -text "Window Width"
        scale $winlevel.ww.s -variable vol_width \
            -from 0 -to 9999 -length 130 -width 15 \
            -showvalue false -orient horizontal \
            -command "$this change_volume_window_width_and_level"
        Tooltip $winlevel.ww.s "Control the window width of\nthe volume rendering"
        bind $winlevel.ww.s <ButtonRelease> "$this execute_vol_ren"
        entry $winlevel.ww.e -textvariable vol_width 
	bind $winlevel.ww.e <Return> "$this change_volume_window_width_and_level 1; $mods(ViewSlices)-c background_thresh"
        pack $winlevel.ww.l $winlevel.ww.s $winlevel.ww.e -side left

        label $winlevel.wl.l -text "Window Level "
        scale $winlevel.wl.s -variable vol_level \
            -from 0 -to 9999 -length 130 -width 15 \
            -showvalue false -orient horizontal \
            -command "$this change_volume_window_width_and_level"
        Tooltip $winlevel.wl.s "Control the window width of\nthe volume rendering"
       bind $winlevel.wl.s <ButtonRelease> "$this execute_vol_ren"
       entry $winlevel.wl.e -textvariable vol_level
       bind $winlevel.wl.e <Return> "$this change_volume_window_width_and_level 1; $mods(ViewSlices)-c background_thresh"
        pack $winlevel.wl.l $winlevel.wl.s $winlevel.wl.e -side left

        trace variable $mods(ViewSlices)-min w "$this update_volume_window_level_scales"
        trace variable $mods(ViewSlices)-max w "$this update_volume_window_level_scales"
       
        #-----------------------------------------------------------
        # Transfer Function Widgets
        #-----------------------------------------------------------
	frame $page.buttons -bd 0
        button $page.buttons.paint -text "Add Paint Layer" \
           -command "$mods(EditColorMap2D)-c addpaint"
        button $page.buttons.undo -text "Undo Paint Stroke" \
           -command "$mods(ViewSlices)-c undo"
	pack $page.buttons.paint $page.buttons.undo -side left \
           -fill x -padx 10 -pady 3 -expand 1
        pack $page.buttons -side top -expand 0 -padx 0 -fill x -pady 3

        $mods(EditColorMap2D) label_widget_columns $page.widgets_label
        pack $page.widgets_label -side top -fill x -padx 2
        iwidgets::scrolledframe $page.widgets -hscrollmode none \
	    -vscrollmode static

        pack $page.widgets -side top -fill both -expand yes -padx 2
        $mods(EditColorMap2D) add_frame [$page.widgets childsite]


        ### Renderer Options Tab
	create_viewer_tab $vis "3D Options"

        $vis.tnb view "Planes"


	### Attach/Detach button
            frame $m.d 
	    pack $m.d -side left -anchor e
            for {set i 0} {$i<42} {incr i} {
                button $m.d.cut$i -text " | " -borderwidth 0 \
                    -foreground "gray25" \
                    -activeforeground "gray25" \
                    -command "$this switch_V_frames" 
	        pack $m.d.cut$i -side top -anchor se -pady 0 -padx 0
		if {$case == 0} {
		    Tooltip $m.d.cut$i $tips(VisAttachHashes)
		} else {
		    Tooltip $m.d.cut$i $tips(VisDetachHashes)
		}
            }
	}
    }

    method toggle_compute_shading {} {
        set ChooseNrrdLighting [lindex [lindex $filters(0) $modules] 36]
        global [set ChooseNrrdLighting]-port-index

        if {[set [set ChooseNrrdLighting]-port-index] == 1} {
	    # lighing computed
	    .standalone.detachedV.f.vis.childsite.tnb.canvas.notebook.cs.page2.cs.shading \
		configure -state normal
	    .standalone.attachedV.f.vis.childsite.tnb.canvas.notebook.cs.page2.cs.shading \
		configure -state normal
	} else {
	    # lighting not computed
	    .standalone.detachedV.f.vis.childsite.tnb.canvas.notebook.cs.page2.cs.shading \
		configure -state disabled
	    .standalone.attachedV.f.vis.childsite.tnb.canvas.notebook.cs.page2.cs.shading \
		configure -state disabled
	}
        [set ChooseNrrdLighting]-c needexecute
    }
    

    ##########################
    ### switch_P_frames
    ##########################
    # This method is called when the user wants to attach or detach
    # the processing frame.
    method switch_P_frames {} {
	set c_width [winfo width $win]
	set c_height [winfo height $win]
	
    	set x [winfo x $win]
	set y [expr [winfo y $win] - 20]
	
	if { $IsPAttached } {	    
	    pack forget $attachedPFr
	    set new_width [expr $c_width - $process_width]
	    append geom1 $new_width x $c_height + [expr $x+$process_width] + $y
            wm geometry $win $geom1 
	    append geom2 $process_width x $c_height + [expr $x-20] + $y
	    wm geometry $detachedPFr $geom2
	    wm deiconify $detachedPFr
	    set IsPAttached 0

	} else {
	    wm withdraw $detachedPFr
	    pack $attachedPFr -anchor n -side left -before $win.viewers
	    set new_width [expr $c_width + $process_width]
            append geom $new_width x $c_height + [expr $x - $process_width] + $y
	    wm geometry $win $geom
	    set IsPAttached 1
	}
    }


    ##########################
    ### switch_V_frames
    ##########################
    # This method is called when the user wants to attach or detach
    # the visualization frame.
    method switch_V_frames {} {
	set c_width [winfo width $win]
	set c_height [winfo height $win]

      	set x [winfo x $win]
	set y [expr [winfo y $win] - 20]

	if { $IsVAttached } {
	    pack forget $attachedVFr
	    set new_width [expr $c_width - $vis_width]
	    append geom1 $new_width x $c_height
            wm geometry $win $geom1
	    set move [expr $c_width - $vis_width]
	    append geom2 $vis_width x $c_height + [expr $x + $move + 20] + $y
	    wm geometry $detachedVFr $geom2
	    wm deiconify $detachedVFr
	    set IsVAttached 0
	} else {
	    wm withdraw $detachedVFr
	    pack $attachedVFr -anchor n -side left -after $win.viewers 
	    set new_width [expr $c_width + $vis_width]
            append geom $new_width x $c_height
	    wm geometry $win $geom
	    set IsVAttached 1
	}
    }

    method set_cur_data_tab {which} {
	if {$initialized} {
	    set cur_data_tab $which
	}
    }


    ############################
    ### change_vis_frame
    ############################
    # Method called when Visualization tabs are changed from
    # the standard options to the global viewer controls
    method change_vis_frame { which } {

	$this check_crop

	# change tabs for attached and detached
        if {$initialized != 0} {
	    if {$which == "Planes"} {
		# Vis Options
		$vis_frame_tab1 view "Planes"
		$vis_frame_tab2 view "Planes"
		set c_vis_tab "Planes"
	    } elseif {$which == "Volume Rendering"} {
		# Vis Options
		$vis_frame_tab1 view "Volume Rendering"
		$vis_frame_tab2 view "Volume Rendering"
		set c_vis_tab "Volume Rendering"
	    } else {
 		$vis_frame_tab1 view "3D Options"
 		$vis_frame_tab2 view "3D Options"
		set c_vis_tab "3D Options"
	    }
	}
    }

    method add_insert_bar {f which} {
	# Add a bar that when a user clicks, will bring
	# up the menu of filters to insert

  	set image_dir [netedit getenv SCIRUN_SRCDIR]/pixmaps
  	set insert [image create photo -file ${image_dir}/powerapp-insertbar.ppm]

  	button $f.insertbar -image $insert \
  	    -anchor n \
  	    -relief sunken -borderwidth 0 
         grid config $f.insertbar -row 1 -column 1 -sticky "n" -pady 0
	
 	bind $f.insertbar <ButtonPress-1> "app popup_insert_menu %X %Y $which"
 	bind $f.insertbar <ButtonPress-2> "app popup_insert_menu %X %Y $which"
 	bind $f.insertbar <ButtonPress-3> "app popup_insert_menu %X %Y $which"
        Tooltip $f.insertbar "Click on this bar to insert any of the\npre-processing filters at this location"
    }

    method popup_insert_menu {x y which} {
	set mouseX $x
	set mouseY $y
	set menu_id ".standalone.insertmenu"
	$this generate_insert_menu $menu_id $which
	tk_popup $menu_id $x $y
    }

    method generate_insert_menu {menu_id which} {
	set num_entries [$menu_id index end]
	if { $num_entries == "none" } { 
	    set num_entries 0
	}
	for {set c 0} {$c <= $num_entries} {incr c } {
	    $menu_id delete 0
	}
	
	$menu_id add command -label "Insert Resample" -command "$this add_Resample $which"
	$menu_id add command -label "Insert Crop" -command "$this add_Crop $which"
	$menu_id add command -label "Insert Histogram" -command "$this add_Histo $which"
	$menu_id add command -label "Insert Cmedian" -command "$this add_Cmedian $which"
    }

    method execute_Data {} {
	# execute the appropriate reader
	
	set ChooseNrrd  [lindex [lindex $filters(0) $modules] $load_choose_input]
        global [set ChooseNrrd]-port-index
        set port [set [set ChooseNrrd]-port-index]
        set mod ""
        if {$port == 0} {
	    # Nrrd
            set mod [lindex [lindex $filters(0) $modules] $load_nrrd]
	} elseif {$port == 1} {
	    # Dicom
            set mod [lindex [lindex $filters(0) $modules] $load_dicom]
	} elseif {$port == 2} {
	    # Analyze
            set mod [lindex [lindex $filters(0) $modules] $load_analyze]
	} else {
	    #Field
            set mod [lindex [lindex $filters(0) $modules] $load_field]
	}

	$mod-c needexecute
        set has_executed 1
    }


    method add_Resample {which} {
	# a which of -1 indicates to add to the end
	if {$which == -1} {
	    set which [expr $grid_rows-1]
        }

        $this check_crop

	global mods

	# Figure out what choose port to use
	set choose [$this determine_choose_port]

	# add modules
	set m1 [addModuleAtPosition "Teem" "UnuNtoZ" "UnuResample" 100 [expr 10 * $num_filters + 500] ]
	
	# add connection to Choose module and new module
	set ChooseNrrd [lindex [lindex $filters(0) $modules] $load_choose_vis]
	set output_mod [lindex [lindex $filters($which) $output] 0]
	set output_port [lindex [lindex $filters($which) $output] 1]
	addConnection $output_mod $output_port $m1 0
	addConnection $m1 0 $ChooseNrrd $choose

	set row $grid_rows
	# if inserting, disconnect current to current's next and connect current
	# to new and new to current's next
	set insert 0
	if {[lindex $filters($which) $next_index] != "end"} {
            set insert 1
	    set n [lindex $filters($which) $next_index] 
	    set next_mod [lindex [lindex $filters($n) $input] 0]
	    set next_port [lindex [lindex $filters($n) $input] 1]

	    set current_mod [lindex [lindex $filters($which) $output] 0]
	    set current_port [lindex [lindex $filters($which) $output] 1]

	    destroyConnection "$current_mod $current_port $next_mod $next_port"
	    addConnection $m1 0 $next_mod $next_port
	    
	    set row [expr [lindex $filters($which) $which_row] + 1]
	    
	    $this move_down_filters $row
	}

        # add to filters array
        set filters($num_filters) [list resample [list $m1] [list $m1 0] [list $m1 0] $which [lindex $filters($which) $next_index] $choose $row 1 "Resample - Unknown"]


	# Make current frame regular
	set p $which.f$which
	$history0.$p configure -background grey75 -foreground black -borderwidth 2
	$history1.$p configure -background grey75 -foreground black -borderwidth 2

        set f0 [add_Resample_UI $history0 $row $num_filters]
        set f1 [add_Resample_UI $history1 $row $num_filters]

        # Add insert bar
        $this add_insert_bar $f0 $num_filters
        $this add_insert_bar $f1 $num_filters

        if {!$insert} {
	    $attachedPFr.f.p.sf justify bottom
	    $detachedPFr.f.p.sf justify bottom
	}

	# Update choose port if
        global eye
        set eye $num_filters
        $this change_eye $num_filters

	# update vars
	set filters($which) [lreplace $filters($which) $next_index $next_index $num_filters]

        #change_current $num_filters

	set num_filters [expr $num_filters + 1]
	set grid_rows [expr $grid_rows + 1]

        change_indicator_labels "Press Update to Resample Volume..."

        $this enable_update 1 2 3
    }


    method add_Crop {which} {
	# a which of -1 indicates to add to the end
	if {$which == -1} {
	    set which [expr $grid_rows-1]
        }

        $this check_crop

	global mods

	# Figure out what choose port to use
	set choose [$this determine_choose_port]

	# add modules
	set m1 [addModuleAtPosition "Teem" "UnuAtoM" "UnuCrop" 100 [expr 10 * $num_filters + 500] ]
	set m2 [addModuleAtPosition "Teem" "NrrdData" "NrrdInfo" 300 [expr 10 * $num_filters + 500] ]
	
	# add connection to Choose module and new module
	set ChooseNrrd [lindex [lindex $filters(0) $modules] $load_choose_vis]
	set output_mod [lindex [lindex $filters($which) $output] 0]
	set output_port [lindex [lindex $filters($which) $output] 1]
	addConnection $output_mod $output_port $m1 0
	addConnection $output_mod $output_port $m2 0
	addConnection $m1 0 $ChooseNrrd $choose

	set row $grid_rows
	# if inserting, disconnect current to current's next and connect current
	# to new and new to current's next
	set insert 0
	if {[lindex $filters($which) $next_index] != "end"} {
            set insert 1
	    set n [lindex $filters($which) $next_index] 
	    set next_mod [lindex [lindex $filters($n) $input] 0]
	    set next_port [lindex [lindex $filters($n) $input] 1]

	    set current_mod [lindex [lindex $filters($which) $output] 0]
	    set current_port [lindex [lindex $filters($which) $output] 1]

	    destroyConnection "$current_mod $current_port $next_mod $next_port"
	    addConnection $m1 0 $next_mod $next_port
	    
	    set row [expr [lindex $filters($which) $which_row] + 1]
	    
	    $this move_down_filters $row
	}

        # add to filters array
        set filters($num_filters) [list crop [list $m1 $m2] [list $m1 0] [list $m1 0] $which [lindex $filters($which) $next_index] $choose $row 1 "Crop - Unknown" [list 0 0 0] 0 [list 0 0 0 0 0 0]]

	# Make current frame regular
	set p $which.f$which
	$history0.$p configure -background grey75 -foreground black -borderwidth 2
	$history1.$p configure -background grey75 -foreground black -borderwidth 2

        set f0 [add_Crop_UI $history0 $row $num_filters]
        set f1 [add_Crop_UI $history1 $row $num_filters]

        # Add insert bar
        $this add_insert_bar $f0 $num_filters
        $this add_insert_bar $f1 $num_filters

        # update crop values to be actual bounds (not M) if available
        global $m1-maxAxis0
        global $m1-maxAxis1
        global $m1-maxAxis2
        if {$has_executed == 1} {
	    global $mods(ViewSlices)-crop_maxAxis0
            global $mods(ViewSlices)-crop_maxAxis1
            global $mods(ViewSlices)-crop_maxAxis2
            set $m1-maxAxis0 [set $mods(ViewSlices)-crop_maxAxis0]
            set $m1-maxAxis1 [set $mods(ViewSlices)-crop_maxAxis1]
            set $m1-maxAxis2 [set $mods(ViewSlices)-crop_maxAxis2]     
        } 
#         else {
#             set $m1-maxAxis0 "M"
#             set $m1-maxAxis1 "M"
# 	    set $m1-maxAxis2 "M"
#         }

        # put traces on changing these

        if {!$insert} {
	    $attachedPFr.f.p.sf justify bottom
	    $detachedPFr.f.p.sf justify bottom
	}

	# Update choose port if
        global eye
        set eye $num_filters
        $this change_eye $num_filters

	# update vars
	set filters($which) [lreplace $filters($which) $next_index $next_index $num_filters]

	set num_filters [expr $num_filters + 1]
	set grid_rows [expr $grid_rows + 1]

        change_indicator_labels "Press Update to Crop Volume..."

        $this enable_update 1 2 3

        if {!$loading} {
	    set current_crop [expr $num_filters-1]
	    $m2-c needexecute
            $this start_crop [expr $num_filters-1]
        }
    }
    
    method add_Cmedian {which} {
	# a which of -1 indicates to add to the end
	if {$which == -1} {
	    set which [expr $grid_rows-1]
        }

        $this check_crop

	global mods

	# Figure out what choose port to use
	set choose [$this determine_choose_port]

	# add modules
	set m1 [addModuleAtPosition "Teem" "UnuAtoM" "UnuCmedian" 100 [expr 10 * $num_filters + 500] ]
	
	# add connection to Choose module and new module
	set ChooseNrrd [lindex [lindex $filters(0) $modules] $load_choose_vis]
	set output_mod [lindex [lindex $filters($which) $output] 0]
	set output_port [lindex [lindex $filters($which) $output] 1]
	addConnection $output_mod $output_port $m1 0
	addConnection $m1 0 $ChooseNrrd $choose

	set row $grid_rows
	# if inserting, disconnect current to current's next and connect current
	# to new and new to current's next
	set insert 0
	if {[lindex $filters($which) $next_index] != "end"} {
            set insert 1
	    set n [lindex $filters($which) $next_index] 
	    set next_mod [lindex [lindex $filters($n) $input] 0]
	    set next_port [lindex [lindex $filters($n) $input] 1]

	    set current_mod [lindex [lindex $filters($which) $output] 0]
	    set current_port [lindex [lindex $filters($which) $output] 1]

	    destroyConnection "$current_mod $current_port $next_mod $next_port"
	    addConnection $m1 0 $next_mod $next_port
	    
	    set row [expr [lindex $filters($which) $which_row] + 1]
	    
	    $this move_down_filters $row
	}

        # add to filters array
        set filters($num_filters) [list cmedian [list $m1] [list $m1 0] [list $m1 0] $which [lindex $filters($which) $next_index] $choose $row 1 "Cmedian - Unknown"]

	# Make current frame regular
	set p $which.f$which
	$history0.$p configure -background grey75 -foreground black -borderwidth 2
	$history1.$p configure -background grey75 -foreground black -borderwidth 2

        set f0 [add_Cmedian_UI $history0 $row $num_filters]
        set f1 [add_Cmedian_UI $history1 $row $num_filters]

        # Add insert bar
        $this add_insert_bar $f0 $num_filters
        $this add_insert_bar $f1 $num_filters

        if {!$insert} {
	    $attachedPFr.f.p.sf justify bottom
	    $detachedPFr.f.p.sf justify bottom
	}

	# Update choose port if
        global eye
        set eye $num_filters
        $this change_eye $num_filters

	# update vars
	set filters($which) [lreplace $filters($which) $next_index $next_index $num_filters]

	set num_filters [expr $num_filters + 1]
	set grid_rows [expr $grid_rows + 1]

        change_indicator_labels "Press Update to Perform Median/Mode Filtering..."

        $this enable_update 1 2 3

    }

    method add_Histo {which} {
	# a which of -1 indicates to add to the end
	if {$which == -1} {
	    set which [expr $grid_rows-1]
        }

        $this check_crop

	global mods

	# Figure out what choose port to use
	set choose [$this determine_choose_port]

	# add modules
	set m1 [addModuleAtPosition "Teem" "UnuAtoM" "UnuHeq"  200 [expr 10 * $num_filters + 400] ]
	set m2 [addModuleAtPosition "Teem" "UnuNtoZ" "UnuQuantize"  200 [expr 10 * $num_filters + 600] ]
	set m3 [addModuleAtPosition "Teem" "Converters" "NrrdToField"  100 [expr 10 * $num_filters + 400] ]
        set m4 [addModuleAtPosition "SCIRun" "FieldsOther" "ScalarFieldStats"  100 [expr 10 * $num_filters + 600] ]

	
	# add connection to Choose module and new module
	set ChooseNrrd [lindex [lindex $filters(0) $modules] $load_choose_vis]
	set output_mod [lindex [lindex $filters($which) $output] 0]
	set output_port [lindex [lindex $filters($which) $output] 1]
	addConnection $output_mod $output_port $m3 2
	addConnection $m3 0 $m4 0
	addConnection $output_mod $output_port $m1 0
	addConnection $m1 0 $m2 0
	addConnection $m2 0 $ChooseNrrd $choose

        global $mods(ViewSlices)-min
        global $mods(ViewSlices)-max
        set min [set $mods(ViewSlices)-min]
        set max [set $mods(ViewSlices)-max]

        if {$min == -1 && $max == -1} {
	    # min/max haven't been set becuase it hasn't executed yet
	    set min 0
	    set max 255
	}

        global $m4-setdata
        set $m4-setdata 1
        global $m4-args
        trace variable $m4-args w "$this update_histo_graph_callback $num_filters"

        global $m1-bins
        set $m1-bins 3000

        global $m2-nbits $m2-minf $m2-maxf $m2-useinputmin $m2-useinputmax
        set $m2-nbits 8
        set $m2-minf $min
        set $m2-maxf $max
        set $m2-useinputmin 1
        set $m2-useinputmax 1


	set row $grid_rows
	# if inserting, disconnect current to current's next and connect current
	# to new and new to current's next
	set insert 0
	if {[lindex $filters($which) $next_index] != "end"} {
            set insert 1
	    set n [lindex $filters($which) $next_index] 
	    set next_mod [lindex [lindex $filters($n) $input] 0]
	    set next_port [lindex [lindex $filters($n) $input] 1]

	    set current_mod [lindex [lindex $filters($which) $output] 0]
	    set current_port [lindex [lindex $filters($which) $output] 1]

	    destroyConnection "$current_mod $current_port $next_mod $next_port"
	    addConnection $m1 0 $next_mod $next_port
	    
	    set row [expr [lindex $filters($which) $which_row] + 1]
	    
	    $this move_down_filters $row
	}

        # add to filters array
        set filters($num_filters) [list histo [list $m1 $m2 $m3 $m4] [list $m1 0] [list $m1 0] $which [lindex $filters($which) $next_index] $choose $row 1 "Histo - Unknown"]

	# Make current frame regular
	set p $which.f$which
	$history0.$p configure -background grey75 -foreground black -borderwidth 2
	$history1.$p configure -background grey75 -foreground black -borderwidth 2

        set f0 [add_Histo_UI $history0 $row $num_filters]
        set f1 [add_Histo_UI $history1 $row $num_filters]


        # Add insert bar
        $this add_insert_bar $f0 $num_filters
        $this add_insert_bar $f1 $num_filters

        if {!$insert} {
	    $attachedPFr.f.p.sf justify bottom
	    $detachedPFr.f.p.sf justify bottom
	}

	# Update choose port if
        global eye
        set eye $num_filters
        $this change_eye $num_filters

	# update vars
	set filters($which) [lreplace $filters($which) $next_index $next_index $num_filters]

	set num_filters [expr $num_filters + 1]
	set grid_rows [expr $grid_rows + 1]

        # execute histogram part so that is visible to user
        if {$has_executed} {
	    $m4-c needexecute
	}

        change_indicator_labels "Press Update to Perform Histogram Equalization..."

        $this enable_update 1 2 3
    }


    ############################
    ### update_histo_graph_callback
    ############################
    # Called when the ScalarFieldStats updates the graph
    # so we can update ours
    method update_histo_graph_callback {i varname varele varop} {

	global mods
        set ScalarFieldStats [lindex [lindex $filters($i) $modules] 3]
        global [set ScalarFieldStats]-min [set ScalarFieldStats]-max

	global [set ScalarFieldStats]-args
        global [set ScalarFieldStats]-nmin
        global [set ScalarFieldStats]-nmax

	set nmin [set [set ScalarFieldStats]-nmin]
	set nmax [set [set ScalarFieldStats]-nmax]
	set args [set [set ScalarFieldStats]-args]

	if {$args == "?"} {
	    return
	}
        
        # for some reason the other graph will only work if I set temp 
        # instead of using the $i value 
	set temp $i

 	set graph $history1.$i.f$i.childsite.ui.histo.childsite.graph

         if { ($nmax - $nmin) > 1000 || ($nmax - $nmin) < 1e-3 } {
             $graph axis configure y -logscale yes
         } else {
             $graph axis configure y -logscale no
         }

         set min [set [set ScalarFieldStats]-min]
         set max [set [set ScalarFieldStats]-max]
         set xvector {}
         set yvector {}
         set yvector [concat $yvector $args]
         set frac [expr double(1.0/[llength $yvector])]

         $graph configure -barwidth $frac
         $graph axis configure x -min $min -max $max \
             -subdivisions 4 -loose 1 \
             -stepsize 0

         for {set i 0} { $i < [llength $yvector] } {incr i} {
             set val [expr $min + $i*$frac*($max-$min)]
             lappend xvector $val
         }
        
          if { [$graph element exists data] == 1 } {
              $graph element delete data
          }

        $graph element create data -label {} -xdata $xvector -ydata $yvector

# 	## other window
  	 set graph $history0.$temp.f$temp.childsite.ui.histo.childsite.graph

          if { ($nmax - $nmin) > 1000 || ($nmax - $nmin) < 1e-3 } {
              $graph axis configure y -logscale yes
          } else {
              $graph axis configure y -logscale no
          }


          $graph configure -barwidth $frac
          $graph axis configure x -min $min -max $max -subdivisions 4 -loose 1

          for {set i 0} { $i < [llength $yvector] } {incr i} {
              set val [expr $min + $i*$frac*($max-$min)]
              lappend xvector $val
          }
        
           if { [$graph element exists "h"] == 1 } {
               $graph element delete "h"
           }

           $graph element create "h" -xdata $xvector -ydata $yvector

    }

    method update_window_level_scales {varname varele varop} {
	global mods
	global $mods(ViewSlices)-min $mods(ViewSlices)-max

	set min [set $mods(ViewSlices)-min]
	set max [set $mods(ViewSlices)-max]
        set span [expr abs([expr $max-$min])]

	# configure window width scale
	$attachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page1.cs.winlevel.childsite.ww.s \
	    configure -from 0 -to $span
	$detachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page1.cs.winlevel.childsite.ww.s \
	    configure -from 0 -to $span

	# configure window level scale
	$attachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page1.cs.winlevel.childsite.wl.s \
	    configure -from $min -to $max
	$detachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page1.cs.winlevel.childsite.wl.s \
	    configure -from $min -to $max
	
        $this update_volume_window_level_scales 1 2 3
    }

    method change_window_width { val } {
	# sync other windows with axial window width
	global mods
	global $mods(ViewSlices)-axial-viewport0-clut_ww
	global $mods(ViewSlices)-sagittal-viewport0-clut_ww
	global $mods(ViewSlices)-coronal-viewport0-clut_ww

	set val [set $mods(ViewSlices)-axial-viewport0-clut_ww]

	set $mods(ViewSlices)-sagittal-viewport0-clut_ww $val
	set $mods(ViewSlices)-coronal-viewport0-clut_ww $val

	# set windows to be dirty
	$mods(ViewSlices)-c setclut

	$mods(ViewSlices)-c redrawall
      
        # if window levels linked, change the vol_width
        global link_winlevel
        if {$link_winlevel == 1} {
	    global vol_width
            set vol_width $val
	    $this change_volume_window_width_and_level -1
        }
    }

    method change_window_level { val } {
	# sync other windows with axial window level
	global mods
	global $mods(ViewSlices)-axial-viewport0-clut_wl
	global $mods(ViewSlices)-sagittal-viewport0-clut_wl
	global $mods(ViewSlices)-coronal-viewport0-clut_wl

	set v [set $mods(ViewSlices)-axial-viewport0-clut_wl]

	set $mods(ViewSlices)-sagittal-viewport0-clut_wl $v
	set $mods(ViewSlices)-coronal-viewport0-clut_wl $v

	# set windows to be dirty
	$mods(ViewSlices)-c setclut

	$mods(ViewSlices)-c redrawall

        # if window levels linked, change the vol_level
        global link_winlevel
        if {$link_winlevel == 1 && $val != -1} {
	    global vol_level
            set vol_level $val
	    $this change_volume_window_width_and_level -1
        }
    }

     method update_volume_window_level_scales {varname varele varop} {
 	global mods
 	global $mods(ViewSlices)-min $mods(ViewSlices)-max

 	set min [set $mods(ViewSlices)-min]
 	set max [set $mods(ViewSlices)-max]
        set span [expr abs([expr $max-$min])]

 	# configure window width scale
 	$attachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page2.cs.winlevel.childsite.ww.s \
 	    configure -from 0 -to $span
 	$detachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page2.cs.winlevel.childsite.ww.s \
 	    configure -from 0 -to $span

 	# configure window level scale
 	$attachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page2.cs.winlevel.childsite.wl.s \
 	    configure -from $min -to $max
 	$detachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page2.cs.winlevel.childsite.wl.s \
 	    configure -from $min -to $max
	
     }

     method change_volume_window_width_and_level { val } {
	 # Change UnuJhisto and UnuQuantize values
	 global vol_width vol_level
	 
	 set min [expr $vol_level-$vol_width/2]
	 set max [expr $vol_level+$vol_width/2]
	 
	 set UnuQuantize [lindex [lindex $filters(0) $modules] 7] 
         set UnuJhisto [lindex [lindex $filters(0) $modules] 21] 
         global [set UnuQuantize]-maxf [set UnuQuantize]-minf
  	 global [set UnuJhisto]-maxs [set UnuJhisto]-mins

  	 set [set UnuQuantize]-maxf $max
       	 set [set UnuQuantize]-minf $min
       	 set [set UnuJhisto]-maxs "$max nan"
       	 set [set UnuJhisto]-mins "$min nan"

         # if linked, change the ViewSlices window width and level
         global link_winlevel
         if {$link_winlevel == 1 && $val != -1} {
	     global mods
	     global $mods(ViewSlices)-axial-viewport0-clut_ww
	     global $mods(ViewSlices)-axial-viewport0-clut_wl
             set $mods(ViewSlices)-axial-viewport0-clut_ww $vol_width
             set $mods(ViewSlices)-axial-viewport0-clut_wl $vol_level

             # update all windows
             $this change_window_width -1
             $this change_window_level -1
	 }
      }

     method execute_vol_ren {} {
     	# execute modules if volume rendering enabled
 	global show_vol_ren
 	if {$show_vol_ren == 1} {
   	    set UnuQuantize [lindex [lindex $filters(0) $modules] 7] 
            set UnuJhisto [lindex [lindex $filters(0) $modules] 21] 
 	    [set UnuQuantize]-c needexecute
            [set UnuJhisto]-c needexecute
         }
     }

     method execute_vol_ren_when_linked {} {
	 # execute volume rendering modules on change of window
	 # and level only if they are linked
	 global link_winlevel show_vol_ren 
	 if {$link_winlevel == 1 && $show_vol_ren == 1} {
   	    set UnuQuantize [lindex [lindex $filters(0) $modules] 7] 
            set UnuJhisto [lindex [lindex $filters(0) $modules] 21] 
 	    [set UnuQuantize]-c needexecute
            [set UnuJhisto]-c needexecute
	 }
     }

    method link_windowlevels {} {
	global link_winlevel

	if {$link_winlevel == 1} {
	    # Set vol_width and vol_level to Viewimage window width and level
	    global vol_width vol_level mods
	    global $mods(ViewSlices)-axial-viewport0-clut_ww
	    global $mods(ViewSlices)-axial-viewport0-clut_wl

            set vol_width [set $mods(ViewSlices)-axial-viewport0-clut_ww]
            set vol_level [set $mods(ViewSlices)-axial-viewport0-clut_wl]

            $this change_volume_window_width_and_level 1

            # execute the volume rendering if it's on
            $this execute_vol_ren
	} 
    }


     method update_BioImage_shading_button_state {varname varele varop} {
         set VolumeVisualizer [lindex [lindex $filters(0) $modules] 14]

         global [set VolumeVisualizer]-shading-button-state
         
         set path f.vis.childsite.tnb.canvas.notebook.cs.page2.cs.shading
         if {[set [set VolumeVisualizer]-shading-button-state]} {
 	     $attachedVFr.$path configure -fg "black"
 	     $detachedVFr.$path configure -fg "black"
 	 } else {
 	     $attachedVFr.$path configure -fg "darkgrey"
 	     $detachedVFr.$path configure -fg "darkgrey"
 	 }
     }

    method check_crop {} {
	if {$turn_off_crop == 1} {
	    $this stop_crop
	}
    }

    method start_crop {which} {
        global mods

        if {!$loading && [lindex $filters($which) $filter_type] == "crop"} {
	    if {$turn_off_crop == 0} {
	       global $mods(ViewSlices)-crop_minPadAxis0 $mods(ViewSlices)-crop_maxPadAxis0
            global $mods(ViewSlices)-crop_minPadAxis1 $mods(ViewSlices)-crop_maxPadAxis1
	    global $mods(ViewSlices)-crop_minPadAxis2 $mods(ViewSlices)-crop_maxPadAxis2

            # turn on Cropping widgets in ViewSlices windows
	    # corresponding with padding values from filter $which
            set pad_vals [lindex $filters($which) 12]
            set $mods(ViewSlices)-crop_minPadAxis0 [lindex $pad_vals 0]
	    set $mods(ViewSlices)-crop_maxPadAxis0 [lindex $pad_vals 1]
	    set $mods(ViewSlices)-crop_minPadAxis1 [lindex $pad_vals 2]
	    set $mods(ViewSlices)-crop_maxPadAxis1 [lindex $pad_vals 3]
	    set $mods(ViewSlices)-crop_minPadAxis2 [lindex $pad_vals 4]
	    set $mods(ViewSlices)-crop_maxPadAxis2 [lindex $pad_vals 5]

            # if crop values are all 0, this is the first time using the crop
            # and the values should be set to bounding box
            set first_time 0
   	    set UnuCrop [lindex [lindex $filters($which) $modules] 0]
	    global [set UnuCrop]-minAxis0 [set UnuCrop]-maxAxis0
	    global [set UnuCrop]-minAxis1 [set UnuCrop]-maxAxis1
	    global [set UnuCrop]-minAxis2 [set UnuCrop]-maxAxis2

            if {[set [set UnuCrop]-minAxis0] == 0 && [set [set UnuCrop]-maxAxis0] == 0 &&
                [set [set UnuCrop]-minAxis1] == 0 && [set [set UnuCrop]-maxAxis1] == 0 &&
                [set [set UnuCrop]-minAxis2] == 0 && [set [set UnuCrop]-maxAxis2] == 0} {
                set first_time 1
            }

            if {$first_time == 0} {
		set enter_crop 1
            }
                

            # should set ViewSlices crop vals if they have been set
            if {$needs_update == 1 && !$first_time} {
                set updating_crop_ui 1
		update

		global $mods(ViewSlices)-crop_minAxis0 $mods(ViewSlices)-crop_maxAxis0
		global $mods(ViewSlices)-crop_minAxis1 $mods(ViewSlices)-crop_maxAxis1
		global $mods(ViewSlices)-crop_minAxis2 $mods(ViewSlices)-crop_maxAxis2

                $mods(ViewSlices)-c startcrop 1
		set $mods(ViewSlices)-crop_minAxis0 [set [set UnuCrop]-minAxis0]
		set $mods(ViewSlices)-crop_minAxis1 [set [set UnuCrop]-minAxis1]
		set $mods(ViewSlices)-crop_minAxis2 [set [set UnuCrop]-minAxis2]
		set $mods(ViewSlices)-crop_maxAxis0 [set [set UnuCrop]-maxAxis0]
		set $mods(ViewSlices)-crop_maxAxis1 [set [set UnuCrop]-maxAxis1]
		set $mods(ViewSlices)-crop_maxAxis2 [set [set UnuCrop]-maxAxis2]
                $mods(ViewSlices)-c updatecrop
            } else {
                $mods(ViewSlices)-c startcrop
            }
            set enter_crop 0
	    set turn_off_crop 1
	    set updating_crop_ui 0
	    set current_crop $which
}
        } else {
	    tk_messageBox -message "No crop widget specified" -type ok -icon info -parent .standalone
	    return
        }

    }

    method update_crop_widget {which type i} {
	global mods

	# get values from UnuCrop, then
	# set ViewSlices crop values
 	set UnuCrop [lindex [lindex $filters($which) $modules] 0]
        set updating_crop_ui 1
        if {$type == "min"} {
    	    global [set UnuCrop]-minAxis$i $mods(ViewSlices)-crop_minAxis$i
            set min [set [set UnuCrop]-minAxis$i]
            set $mods(ViewSlices)-crop_minAxisi$ $min           
        } else {
    	    global [set UnuCrop]-maxAxis$i $mods(ViewSlices)-crop_maxAxis$i
            set max [set [set UnuCrop]-maxAxis$i]
            set $mods(ViewSlices)-crop_maxAxisi$ $max 
        }
        set updating_crop_ui 0

	# if crop on, update it
        $this update_crop $which
    }

    method update_crop {which} {
	if {$turn_off_crop == 1} {
	    set updating_crop_ui 1
	    $this stop_crop
	    after 300 "$this start_crop $which"
	}
    }

    method stop_crop {} {
        global mods
        $mods(ViewSlices)-c stopcrop
        set turn_off_crop 0
    }


    method print_filters {} {
	parray filters
    }

    method add_Resample_UI {history row which} {
	frame $history.$which
	grid config $history.$which -column 0 -row $row -sticky "nw" -pady 0

	# Add eye radiobutton
        global eye
	radiobutton $history.$which.eye$which -text "" \
	    -variable eye -value $which \
	    -command "$this change_eye $which"
	Tooltip $history.$which.eye$which "Select to change current view\nof 3D and 2D windows"

	grid config $history.$which.eye$which -column 0 -row 0 -sticky "nw"

	iwidgets::labeledframe $history.$which.f$which \
	    -labeltext "Resample" \
	    -labelpos nw \
	    -borderwidth 2 
	grid config $history.$which.f$which -column 1 -row 0 -sticky "nw"

	set w [$history.$which.f$which childsite]

	frame $w.expand
	pack $w.expand -side top -anchor nw 

	set image_dir [netedit getenv SCIRUN_SRCDIR]/pixmaps
	set show [image create photo -file ${image_dir}/play-icon-small.ppm]
	set close [image create photo -file ${image_dir}/powerapp-close.ppm]
	button $w.expand.b -image $show \
	    -anchor nw \
	    -command "$this change_visibility $which" \
	    -relief flat
	Tooltip $w.expand.b "Click to minimize/show\nthe Resample UI"
	label $w.expand.l -text "Resample - Unknown" -width $label_width \
            -anchor nw
	Tooltip $w.expand.l "Right click to edit label"

 	button $w.expand.c -image $close \
 	    -anchor nw \
 	    -command "$this filter_Delete $which" \
 	    -relief flat

	Tooltip $w.expand.c "Click to delete this filter from\nthe pipeline. All settings\nfor this filter will be lost."

	pack $w.expand.b $w.expand.l $w.expand.c -side left -anchor nw

	bind $w.expand.l <ButtonPress-3> "$this change_label %X %Y $which"
	
	frame $w.ui
	pack $w.ui -side top -expand yes -fill both

        set UnuResample [lindex [lindex $filters($which) $modules] 0]
	for {set i 0} {$i < $dimension} {incr i} {
	    global [set UnuResample]-resampAxis$i
	    if {!$loading_ui} {
               set [set UnuResample]-resampAxis$i "x1"
            }
            trace variable [set UnuResample]-resampAxis$i w "$this enable_update"
	    make_entry $w.ui.$i "Axis $i:" $UnuResample-resampAxis$i $which
	    pack $w.ui.$i -side top -anchor nw -expand yes -fill x

            bind $w.ui.$i <ButtonPress-1> "$this check_crop"
	}

        # configure labels
        $w.ui.0.l configure -text "Sagittal" -width 10
        $w.ui.1.l configure -text "Coronal" -width 10
        $w.ui.2.l configure -text "Axial" -width 10

        global [set UnuResample]-sigma [set UnuResample]-extent
        global [set UnuResample]-filtertype 
        set [set UnuResample]-filtertype cubicBS
        set [set UnuResample]-sigma 2
        set [set UnuResample]-extent 2

 	iwidgets::optionmenu $w.ui.kernel -labeltext "Filter Type:" \
 	    -labelpos w \
            -command "$this change_kernel $w.ui.kernel $which"
 	pack $w.ui.kernel -side top -anchor nw 

 	$w.ui.kernel insert end Box Tent "Cubic (Catmull-Rom)" \
 	    "Cubic (B-spline)" Quartic Gaussian
	
 	$w.ui.kernel select "Cubic (B-spline)"

	return $history.$which
    }



    method add_Crop_UI {history row which} {
	frame $history.$which
	grid config $history.$which -column 0 -row $row -sticky "nw" -pady 0

	# Add eye radiobutton
        global eye
	radiobutton $history.$which.eye$which -text "" \
	    -variable eye -value $which \
	    -command "$this change_eye $which"
	Tooltip $history.$which.eye$which "Select to change current view\nof 3D and 2D windows"

	grid config $history.$which.eye$which -column 0 -row 0 -sticky "nw"

	iwidgets::labeledframe $history.$which.f$which \
	    -labeltext "Crop" \
	    -labelpos nw \
	    -borderwidth 2 
	grid config $history.$which.f$which -column 1 -row 0 -sticky "nw"

	set w [$history.$which.f$which childsite]

	frame $w.expand
	pack $w.expand -side top -anchor nw

	set image_dir [netedit getenv SCIRUN_SRCDIR]/pixmaps
	set show [image create photo -file ${image_dir}/play-icon-small.ppm]
	set close [image create photo -file ${image_dir}/powerapp-close.ppm]
	button $w.expand.b -image $show \
	    -anchor nw \
	    -command "$this change_visibility $which" \
	    -relief flat
	Tooltip $w.expand.b "Click to minimize/show\nthe Crop UI"
	label $w.expand.l -text "Crop - Unknown" -width $label_width \
	    -anchor nw
	Tooltip $w.expand.l "Right click to edit label"


 	button $w.expand.c -image $close \
 	    -anchor nw \
 	    -command "$this filter_Delete $which" \
 	    -relief flat

	Tooltip $w.expand.c "Click to delete this filter from\nthe pipeline. All settings\nfor the filter will be lost."

	pack $w.expand.b $w.expand.l $w.expand.c -side left -anchor nw 

	bind $w.expand.l <ButtonPress-3> "$this change_label %X %Y $which"
	
	frame $w.ui
	pack $w.ui -side top -anchor nw -expand yes -fill x
	
	set UnuCrop [lindex [lindex $filters($which) $modules] 0]
	global [set UnuCrop]-num-axes
	set [set UnuCrop]-num-axes $dimension        

        global [set UnuCrop]-reset_data
        set [set UnuCrop]-reset_data 0
        global [set UnuCrop]-digits_only
        set [set UnuCrop]-digits_only 1

	for {set i 0} {$i < $dimension} {incr i} {
	    global [set UnuCrop]-minAxis$i
	    global [set UnuCrop]-maxAxis$i
            if {!$loading_ui} {
	        set [set UnuCrop]-minAxis$i 0
	    }
  
            trace variable [set UnuCrop]-minAxis$i w "$this enable_update"
            trace variable [set UnuCrop]-maxAxis$i w "$this enable_update"

	    frame $w.ui.$i
	    pack $w.ui.$i -side top -anchor nw -expand yes -fill x

	    label $w.ui.$i.minl -text "Min Axis $i:"
	    entry $w.ui.$i.minv -textvariable [set UnuCrop]-minAxis$i \
		-width 4
	    label $w.ui.$i.maxl -text "Max Axis $i:"
	    entry $w.ui.$i.maxv -textvariable [set UnuCrop]-maxAxis$i \
		-width 4

            bind $w.ui.$i.minv <ButtonPress-1> "$this start_crop $which"
            bind $w.ui.$i.maxv <ButtonPress-1> "$this start_crop $which"
            bind $w.ui.$i.minv <Return> "$this update_crop_widget $which min $i"
            bind $w.ui.$i.maxv <Return> "$this update_crop_widget $which max $i"

            grid configure $w.ui.$i.minl -row $i -column 0 -sticky "w"
            grid configure $w.ui.$i.minv -row $i -column 1 -sticky "e"
            grid configure $w.ui.$i.maxl -row $i -column 2 -sticky "w"
            grid configure $w.ui.$i.maxv -row $i -column 3 -sticky "e"
	}

        # Configure labels
        $w.ui.0.minl configure -text "Right:" -width 10
        $w.ui.0.maxl configure -text "Left:" -width 10

        $w.ui.1.minl configure -text "Posterior:" -width 10
        $w.ui.1.maxl configure -text "Anterior:" -width 10

        $w.ui.2.minl configure -text "Inferior:" -width 10
        $w.ui.2.maxl configure -text "Superior:" -width 10

	return $history.$which
    }

    method set_pads { which n0 x0 n1 x1 n2 x2 } {
	set filters($which) [lreplace $filters($which) 10 10 [list $n0 $x0 $n1 $x1 $n2 $x2]
    }


    method update_crop_values { varname varele varop } {
 	    global mods 

	if {$current_crop != -1 && $updating_crop_ui == 0 && $enter_crop == 0} {
 	    # verify that a crop is selected
 	    if {[lindex $filters($current_crop) $filter_type] != "crop"} {
                 return
            }
             # determine which crop to update
             set UnuCrop [lindex [lindex $filters($current_crop) $modules] 0]
          
             # get list of pad values
             set pad_vals [lindex $filters($current_crop) 12]

             # update the correct UnuCrop variable
             if {[string first "crop_minAxis0" $varname] != -1} {
		 global [set UnuCrop]-minAxis0
                 global $mods(ViewSlices)-crop_minAxis0
                 set [set UnuCrop]-minAxis0 [expr [set $mods(ViewSlices)-crop_minAxis0] + [lindex $pad_vals 0]]
             } elseif {[string first "crop_maxAxis0" $varname] != -1} {
		 global [set UnuCrop]-maxAxis0
                 global $mods(ViewSlices)-crop_maxAxis0
                 #set [set UnuCrop]-maxAxis0 [expr [set $mods(ViewSlices)-crop_maxAxis0]+ [lindex $pad_vals 1]] 
                 set [set UnuCrop]-maxAxis0 [set $mods(ViewSlices)-crop_maxAxis0] 
             } elseif {[string first "crop_minAxis1" $varname] != -1} {
		 global [set UnuCrop]-minAxis1
                 global $mods(ViewSlices)-crop_minAxis1
                 set [set UnuCrop]-minAxis1 [expr [set $mods(ViewSlices)-crop_minAxis1] + [lindex $pad_vals 2]]
 	    } elseif {[string first "crop_maxAxis1" $varname] != -1} {
 	        global [set UnuCrop]-maxAxis1
		global $mods(ViewSlices)-crop_maxAxis1
		#set [set UnuCrop]-maxAxis1 [expr [set $mods(ViewSlices)-crop_maxAxis1] + [lindex $pad_vals 3]]
                 set [set UnuCrop]-maxAxis1 [set $mods(ViewSlices)-crop_maxAxis1]
             } elseif {[string first "crop_minAxis2" $varname] != -1} {
		 global [set UnuCrop]-minAxis2
                 global $mods(ViewSlices)-crop_minAxis2
                 set [set UnuCrop]-minAxis2 [expr [set $mods(ViewSlices)-crop_minAxis2] + [lindex $pad_vals 4]]
 	    } elseif {[string first "crop_maxAxis2" $varname] != -1} {
 	        global [set UnuCrop]-maxAxis2
		global $mods(ViewSlices)-crop_maxAxis2
		#set [set UnuCrop]-maxAxis2 [expr [set $mods(ViewSlices)-crop_maxAxis2] + [lindex $pad_vals 5]]
                set [set UnuCrop]-maxAxis2 [set $mods(ViewSlices)-crop_maxAxis2]
             }
         }
    }


    method add_Cmedian_UI {history row which} {
	frame $history.$which
	grid config $history.$which -column 0 -row $row -sticky "nw" -pady 0

	# Add eye radiobutton
	global eye
	radiobutton $history.$which.eye$which -text "" \
	    -variable eye -value $which \
	    -command "$this change_eye $which"
	Tooltip $history.$which.eye$which "Select to change current view\nof 3D and 2D windows"

	grid config $history.$which.eye$which -column 0 -row 0 -sticky "nw"

	iwidgets::labeledframe $history.$which.f$which \
	    -labeltext "Cmedian" \
	    -labelpos nw \
	    -borderwidth 2 
	grid config $history.$which.f$which -column 1 -row 0 -sticky "nw"

	set w [$history.$which.f$which childsite]

	frame $w.expand
	pack $w.expand -side top -anchor nw

	set image_dir [netedit getenv SCIRUN_SRCDIR]/pixmaps
	set show [image create photo -file ${image_dir}/play-icon-small.ppm]
	set close [image create photo -file ${image_dir}/powerapp-close.ppm]
	button $w.expand.b -image $show \
	    -anchor nw \
	    -command "$this change_visibility $which" \
	    -relief flat
        Tooltip $w.expand.b "Click to minimize/show\nthe Cmedian UI"
	label $w.expand.l -text "Cmedian - Unknown" -width $label_width \
	    -anchor nw
	Tooltip $w.expand.l "Right click to edit label"

 	button $w.expand.c -image $close \
 	    -anchor nw \
 	    -command "$this filter_Delete $which" \
 	    -relief flat

	Tooltip $w.expand.c "Click to delete this filter from\nthe pipeline. All settings\nfor the filter will be lost."

	pack $w.expand.b $w.expand.l $w.expand.c -side left -anchor nw 

	bind $w.expand.l <ButtonPress-3> "$this change_label %X %Y $which"
	
	frame $w.ui
	pack $w.ui -side top -anchor nw -expand yes -fill x
	
        set UnuCmedian [lindex [lindex $filters($which) $modules] 0]
	global [set UnuCmedian]-radius
        trace variable [set UnuCmedian]-radius w "$this enable_update"

	frame $w.ui.radius
	pack $w.ui.radius -side top -anchor nw -expand yes -fill x
	label $w.ui.radius.l -text "Radius:"
	entry $w.ui.radius.v -textvariable [set UnuCmedian]-radius \
	    -width 6

        bind $w.ui.radius.v <ButtonPress-1> "$this check_crop"

	pack $w.ui.radius.l $w.ui.radius.v -side left -anchor nw \
	    -expand yes -fill x

	return $history.$which
    }

    method add_Histo_UI {history row which} {
	frame $history.$which
       	grid config $history.$which -column 0 -row $row -sticky "nw" -pady 0
	
	# Add eye radiobutton
	global eye
	radiobutton $history.$which.eye$which -text "" \
	    -variable eye -value $which \
	    -command "$this change_eye $which"

	grid config $history.$which.eye$which -column 0 -row 0 -sticky "nw"

	iwidgets::labeledframe $history.$which.f$which \
	    -labeltext "Histogram" \
	    -labelpos nw \
	    -borderwidth 2 
	grid config $history.$which.f$which -column 1 -row 0 -sticky "nw"

	set w [$history.$which.f$which childsite]

	frame $w.expand
	pack $w.expand -side top -anchor nw

	set image_dir [netedit getenv SCIRUN_SRCDIR]/pixmaps
	set show [image create photo -file ${image_dir}/play-icon-small.ppm]
	set close [image create photo -file ${image_dir}/powerapp-close.ppm]
	button $w.expand.b -image $show \
	    -anchor nw \
	    -command "$this change_visibility $which" \
	    -relief flat
	label $w.expand.l -text "Histogram - Unknown" -width $label_width \
	    -anchor nw

 	button $w.expand.c -image $close \
 	    -anchor nw \
 	    -command "$this filter_Delete $which" \
 	    -relief flat

	Tooltip $w.expand.c "Click to delete this filter from\nthe pipeline. All settings\nfor the filter will be lost."

	pack $w.expand.b $w.expand.l $w.expand.c -side left -anchor nw 

	bind $w.expand.l <ButtonPress-3> "$this change_label %X %Y $which"
	
	frame $w.ui
	pack $w.ui -side top -anchor nw -expand yes -fill x
	

        set UnuHeq  [lindex [lindex $filters($which) $modules] 0]
        set UnuQuantize [lindex [lindex $filters($which) $modules] 1]
        set ScalarFieldStats [lindex [lindex $filters($which) $modules] 3]

        ### Histogram
        iwidgets::labeledframe $w.ui.histo \
            -labelpos nw -labeltext "Histogram"
 	pack $w.ui.histo -side top -fill x -anchor n -expand 1

 	set histo [$w.ui.histo childsite]

        global [set ScalarFieldStats]-min
	global [set ScalarFieldStats]-max
	global [set ScalarFieldStats]-nbuckets

	frame $histo.row1
	pack $histo.row1 -side top

	blt::barchart $histo.graph -title "" \
	    -height [expr [set [set ScalarFieldStats]-nbuckets]*3/5.0] \
	    -width 200 -plotbackground gray80
        pack $histo.graph

	global [set UnuHeq]-amount
        trace variable [set UnuHeq]-amount w "$this enable_update"
      
        if {!$loading_ui} {
   	    set [set UnuHeq]-amount 1.0
        }

	scale $w.ui.amount -label "Amount" \
	    -from 0.0 -to 1.0 \
	    -variable [set UnuHeq]-amount \
	    -showvalue true \
	    -orient horizontal \
	    -resolution 0.01
	pack $w.ui.amount -side top -anchor nw -expand yes -fill x

        bind $w.ui.amount <ButtonPress-1> "$this check_crop"

        global [set UnuQuantize]-minf [set UnuQuantize]-maxf
        global [set UnuQuantize]-useinputmin [set UnuQuantize]-useinputmax
        trace variable [set UnuQuantize]-minf w "$this enable_update"
        trace variable [set UnuQuantize]-maxf w "$this enable_update"
        trace variable [set UnuQuantize]-useinputmin w "$this enable_update"
        trace variable [set UnuQuantize]-useinputmax w "$this enable_update"

        frame $w.ui.min -relief groove -borderwidth 2
	pack $w.ui.min -side top -expand yes -fill x

        iwidgets::entryfield $w.ui.min.v -labeltext "Min:" \
	    -textvariable [set UnuQuantize]-minf
        pack $w.ui.min.v -side top -expand yes -fill x
        bind $w.ui.min.v <ButtonPress-1> "$this check_crop"

        checkbutton $w.ui.min.useinputmin \
	    -text "Use lowest value of input nrrd as min" \
	    -variable [set UnuQuantize]-useinputmin
        pack $w.ui.min.useinputmin -side top -expand yes -fill x
        bind $w.ui.min.useinputmin <ButtonPress-1> "$this check_crop"


	frame $w.ui.max -relief groove -borderwidth 2
	pack $w.ui.max -side top -expand yes -fill x

        iwidgets::entryfield $w.ui.max.v -labeltext "Max:" \
	    -textvariable [set UnuQuantize]-maxf
        pack $w.ui.max.v -side top -expand yes -fill x
        bind $w.ui.max.v <ButtonPress-1> "$this check_crop"

        checkbutton $w.ui.max.useinputmax \
	    -text "Use highest value of input nrrd as max" \
	    -variable [set UnuQuantize]-useinputmax
        pack $w.ui.max.useinputmax -side top -expand yes -fill x
        bind $w.ui.max.useinputmax <ButtonPress-1> "$this check_crop"
	
	return $history.$which
    }


    method determine_choose_port {} {
	global Subnet
  	set choose 0
	set ChooseNrrd [lindex [lindex $filters(0) $modules] $load_choose_vis]

        foreach conn $Subnet([set ChooseNrrd]_connections) { ;# all module connections
  	    if {[lindex $conn 2] == $ChooseNrrd} {
  		set choose [expr $choose + 1]
  	    }
  	}
  	return $choose

    }


    method filter_Delete {which} {
	global mods

	# Do not remove Load (0)
	if {$which == 0} {
	    tk_messageBox -message "Cannot delete Load step." -type ok -icon info -parent .standalone
	    return
	}

	set current_row [lindex $filters($which) $which_row]

	# remove ui
	grid remove $history0.$which
	grid remove $history1.$which
	
	set next [lindex $filters($which) $next_index]
	set current_choose [lindex $filters($which) $choose_port]
	
	if {$next != "end"} {
	    move_up_filters [lindex $filters($which) $which_row]
	}

        # delete filter modules
        set l [llength [lindex $filters($which) $modules]]
        for {set i 0} {$i < $l} {incr i} {
            moduleDestroy [lindex [lindex $filters($which) $modules] $i]
        }

        # update choose ports of other filters
        set port [lindex $filters($which) $choose_port]
        $this update_choose_ports $port

	set prev_mod [lindex [lindex $filters([lindex $filters($which) $prev_index]) $output] 0]
	set prev_port [lindex [lindex $filters([lindex $filters($which) $prev_index]) $output] 1]
	set current_mod [lindex [lindex $filters($which) $output] 0]
	set current_port [lindex [lindex $filters($which) $output] 1]

	# add connection from previous to next
	if {$next != "end"} {
	    set next_mod [lindex [lindex $filters([lindex $filters($which) $next_index]) $output] 0]
	    set next_port [lindex [lindex $filters([lindex $filters($which) $next_index]) $output] 1]
	    addConnection $prev_mod $prev_port $next_mod $next_port
	}    

	# set which_row to be -1
	set filters($which) [lreplace $filters($which) $which_row $which_row -1]

	# update prev's next
	set p [lindex $filters($which) $prev_index]
	set n [lindex $filters($which) $next_index]
	set filters($p) [lreplace $filters($p) $next_index $next_index $n]

	# update next's prev (only if not end)
	if {$next != "end"} {
	    set filters($n) [lreplace $filters($n) $prev_index $prev_index $p]
	} 

	# determine next filter to be currently selected
	# by iterating over all valid filters and choosing
	# the one on the previous row
        set ChooseNrrd [lindex [lindex $filters(0) $modules] $load_choose_vis]  
	global $ChooseNrrd-port-index

	set next_row [expr $which_row + 1]
	set next_filter 0

	if {[string equal $next "end"]} {
	    set next_row [expr $which_row - 1]
	} else {
	    set next_row [expr $next_row - 1]
	}

 	for {set i 0} {$i < $num_filters} {incr i} {
 	    # check if it hasn't been deleted
 	    set r [lindex $filters($i) $which_row]
 	    if {$r == $next_row} {
 		set next_filter $i
 	    }
 	}
	set next_choose [lindex $filters($next_filter) $choose_port]

	set $ChooseNrrd--port-index $next_choose

        global eye
        set eye $next_filter
	$this change_eye $next_filter

	set grid_rows [expr $grid_rows - 1]
    }

    method update_changes {} {
	set mod ""

	$this check_crop

        # for any crops in the pipeline that are still active, 
	# save out crop pads for widgets
  	for {set i 1} {$i < $num_filters} {incr i} {
             if {[lindex $filters($i) $filter_type] == "crop" && 
		 [lindex $filters($i) $which_row] != -1} {
                 set UnuCrop [lindex [lindex $filters($i) $modules] 0]

		 set bounds_set [lindex $filters($i) 11]
		 set pad_vals ""
		 if {$bounds_set == 1} {
		     global [set UnuCrop]-minAxis0 [set UnuCrop]-maxAxis0
		     global [set UnuCrop]-minAxis1 [set UnuCrop]-maxAxis1
		     global [set UnuCrop]-minAxis2 [set UnuCrop]-maxAxis2
		     set bounds [lindex $filters($i) 10]
		     set bounds0 [lindex $bounds 0]
		     set bounds1 [lindex $bounds 1]
		     set bounds2 [lindex $bounds 2]

		     set pad_vals [list [set [set UnuCrop]-minAxis0] \
				       [expr $bounds0-[set [set UnuCrop]-maxAxis0]] \
				       [set [set UnuCrop]-minAxis1] \
				       [expr $bounds1-[set [set UnuCrop]-maxAxis1]] \
				       [set [set UnuCrop]-minAxis2] \
				       [expr $bounds2-[set [set UnuCrop]-maxAxis2]]]
		     #$this update_crop $i
		     if {$turn_off_crop == 1} {
			 $this update_crop $i
		     }
		 } else {
		     set pad_vals [list 0 0 0 0 0 0]
		 }
		 set filters($i) [lreplace $filters($i) 12 12 $pad_vals]
  	    }
  	}

	if {$has_executed == 1} {
            if {$grid_rows == 1} {
		$this execute_Data
	    } else {
	        # find first valid filter and execute that
 	        for {set i 1} {$i < $num_filters} {incr i} {
                    if {[info exists filters($i)]} {
		        set tmp_row [lindex $filters($i) $which_row]
		        if {$tmp_row != -1} {
			    set mod [lindex [lindex $filters($i) $input] 0]
 	  	   	    break
	 	        }
		    }
                }
		$mod-c needexecute
            }
	} else {
            $this set_viewer_position
            $this execute_Data
	}

        set has_executed 1

        $this disable_update
    }

    method disable_update {} {
	set needs_update 0
        # grey out update button
        $attachedPFr.f.p.update configure -background "grey75" -state disabled
        $detachedPFr.f.p.update configure -background "grey75" -state disabled
    }

    method enable_update {a b c} {
	set needs_update 1
        # fix  update button
        $attachedPFr.f.p.update configure -background "#008b45" -state normal
        $detachedPFr.f.p.update configure -background "#008b45" -state normal
    }

    method move_down_filters {row} {
	# Since we are inserting, we need to forget the grid rows
	# below us and move them down a row
	set re_pack [list]
	for {set i 1} {$i < $num_filters} {incr i} {
	    if {[info exists filters($i)]} {
		set tmp_row [lindex $filters($i) $which_row]
		if {$tmp_row != -1 && ($tmp_row > $row || $tmp_row == $row)} {
		    grid forget $history0.$i
		    grid forget $history1.$i
		    
		    set filters($i) [lreplace $filters($i) $which_row $which_row [expr $tmp_row + 1] ]		    
		    lappend re_pack $i
		}
	    }
	}
	# need to re configure them after they have all been removed
	for {set i 0} {$i < [llength $re_pack]} {incr i} {
	    set index [lindex $re_pack $i]
	    set new_row [lindex $filters($index) $which_row]
            grid config $history0.$index -row $new_row -column 0 -sticky "nw"
            grid config $history1.$index -row $new_row -column 0 -sticky "nw"
	}
    }

    method move_up_filters {row} {
	# Since we are deleting, we need to forget the grid rows
	# below us and move them up a row
	set re_pack [list]
	for {set i 1} {$i < $num_filters} {incr i} {
	    if {[info exists filters($i)]} {
		set tmp_row [lindex $filters($i) $which_row]
		if {$tmp_row != -1 && $tmp_row > $row } {
		    grid forget $history0.$i
		    grid forget $history1.$i
		    set filters($i) [lreplace $filters($i) $which_row $which_row [expr $tmp_row - 1] ]		    
		    lappend re_pack $i
		}
	    }
	}
	# need to re configure them after they have all been removed
	for {set i 0} {$i < [llength $re_pack]} {incr i} {
	    set index [lindex $re_pack $i]
	    set new_row [lindex $filters($index) $which_row]
            grid config $history0.$index -row $new_row -column 0 -sticky "nw"
            grid config $history1.$index -row $new_row -column 0 -sticky "nw"
	}
    }

    # Decremement any choose ports that are greater than the current port
    method update_choose_ports {port} {
	for {set i 1} {$i < $num_filters} { incr i} {
	    set tmp_row [lindex $filters($i) $which_row]
	    set tmp_port [lindex $filters($i) $choose_port]
		if {$tmp_row != -1 && $tmp_port > $port } {  
                    set filters($i) [lreplace $filters($i) $choose_port $choose_port [expr $tmp_port - 1] ]
                }
	}
    }

    method change_eye {which} {

	$this check_crop

	set ChooseNrrd [lindex [lindex $filters(0) $modules] $load_choose_vis] 
        set port [lindex $filters($which) $choose_port]
        global [set ChooseNrrd]-port-index
        set [set ChooseNrrd]-port-index $port

        $this enable_update 1 2 3
    }

    method change_label {x y which} {

        $this check_crop

	if {![winfo exists .standalone.change_label]} {
	    # bring up ui to type name
	    global new_label
	    set old_label [$history0.$which.f$which.childsite.expand.l cget -text]
	    set new_label $old_label
	    
	    toplevel .standalone.change_label
	    wm minsize .standalone.change_label 150 50
            wm title .standalone.change_label "Change Label"
	    set x [expr $x + 10]
	    wm geometry .standalone.change_label "+$x+$y"
	    
	    label .standalone.change_label.l -text "Please enter a label for this filter."
	    pack .standalone.change_label.l -side top -anchor nw -padx 4 -pady 4
	    
	    frame .standalone.change_label.info
	    pack .standalone.change_label.info -side top -anchor nw
	    
	    label .standalone.change_label.info.l -text "Label:"
	    entry .standalone.change_label.info.e -textvariable new_label 
	    pack .standalone.change_label.info.l .standalone.change_label.info.e -side left -anchor nw \
		-padx 4 -pady 4
	    bind .standalone.change_label.info.e <Return> "destroy .standalone.change_label"

	    frame .standalone.change_label.buttons
	    pack .standalone.change_label.buttons -side top -anchor n
	    
	    button .standalone.change_label.buttons.apply -text "Apply" \
		-command "destroy .standalone.change_label"
	    button .standalone.change_label.buttons.cancel -text "Cancel" \
		-command "global new_label; set new_label CaNceL; destroy .standalone.change_label"
	    pack .standalone.change_label.buttons.apply .standalone.change_label.buttons.cancel -side left \
		-padx 4 -pady 4 -anchor n
	    
	    tkwait window .standalone.change_label
	    
	    if {$new_label != "CaNceL" && $new_label != $old_label} {
		# change label
		$history0.$which.f$which.childsite.expand.l configure -text $new_label
		$history1.$which.f$which.childsite.expand.l configure -text $new_label
		set filters($which) [lreplace $filters($which) $filter_label $filter_label $new_label]
	    }
	} else {
	    SciRaise .standalone.change_label
	}
    }

    method change_visibility {num} {

        $this check_crop

	set visible [lindex $filters($num) $visibility]
	set image_dir [netedit getenv SCIRUN_SRCDIR]/pixmaps
	set show [image create photo -file ${image_dir}/expand-icon-small.ppm]
	set hide [image create photo -file ${image_dir}/play-icon-small.ppm]
	if {$visible == 1} {
	    # hide

	    $history0.$num.f$num.childsite.expand.b configure -image $show
	    $history1.$num.f$num.childsite.expand.b configure -image $show
	    
	    pack forget $history0.$num.f$num.childsite.ui 
	    pack forget $history1.$num.f$num.childsite.ui 

	    set filters($num) [lreplace $filters($num) $visibility $visibility 0]
	} else {
	    # show

	    $history0.$num.f$num.childsite.expand.b configure -image $hide
	    $history1.$num.f$num.childsite.expand.b configure -image $hide

	    pack $history0.$num.f$num.childsite.ui -side top -expand yes -fill both
	    pack $history1.$num.f$num.childsite.ui -side top -expand yes -fill both

	    set filters($num) [lreplace $filters($num) $visibility $visibility 1]
	}
	
    }


    ##################################
    #### change_kernel
    ##################################
    # Update the resampling kernel variable and
    # update the other attached/detached optionmenu
    method change_kernel { w num} {

        $this check_crop

	set UnuResample [lindex [lindex $filters($num) $modules] 0]

        global [set UnuResample]-filtertype
	
	set which [$w get]

	if {$which == "Box"} {
	    set [set UnuResample]-filtertype box
	} elseif {$which == "Tent"} {
	    set [set UnuResample]-filtertype tent
	} elseif {$which == "Cubic (Catmull-Rom)"} {
	    set [set UnuResample]-filtertype cubicCR
	} elseif {$which == "Cubic (B-spline)"} {
	    set [set UnuResample]-filtertype cubicBS
	} elseif {$which == "Quartic"} {
	    set [set UnuResample]-filtertype quartic
	} elseif {$which == "Gaussian"} {
	    set [set UnuResample]-filtertype gaussian
	}

        $this enable_update 1 2 3

	# update attach/detach one
        $history0.$num.f$num.childsite.ui.kernel select $which
	$history1.$num.f$num.childsite.ui.kernel select $which

    }

    method update_kernel { num } {
	set UnuResample [lindex [lindex $filters($num) $modules] 0]

        global [set UnuResample]-filtertype

        set f [set [set UnuResample]-filtertype]
        set t "Box"
  
        if {$f == "box"} {
	    set t "Box"
	} elseif {$f == "tent"} {
	    set t "Tent"
	} elseif {$f == "cubicCR"} {
	    set t "Cubic (Catmull-Rom)"
	} elseif {$f == "cubicBS"} {
	    set t "Cubic (B-spline)"
	} elseif {$f == "quartic"} {
	    set t "Quartic"
	} else {
	    set t "Gaussian"
	}

        $history0.$num.f$num.childsite.ui.kernel select $t
        $history1.$num.f$num.childsite.ui.kernel select $t
    }

    method make_entry {w text v num} {
        frame $w
        label $w.l -text "$text" 
        pack $w.l -side left
        global $v
        entry $w.e -textvariable $v 
        pack $w.e -side right
    }

    ##############################
    ### save_image
    ##############################
    # To be filled in by child class. It should save out the
    # viewer image.
    method save_image {} {
	global mods
	$mods(Viewer)-ViewWindow_0 makeSaveImagePopup
    }

    ##############################
    ### save_session
    ##############################
    # To be filled in by child class. It should save out a session
    # for the specific app.
    method save_session {} {

	global mods

	if {$saveFile == ""} {
	    
	    set types {
		{{App Settings} {.ses} }
		{{Other} { * } }
	    } 
	    set saveFile [ tk_getSaveFile -defaultextension {.ses} \
			       -filetypes $types ]
	}
	if { $saveFile != "" } {
	    # configure title
	    wm title .standalone "BioImage - [getFileName $saveFile]" 

	    # write out regular network
	    writeNetwork $saveFile

	    set fileid [open $saveFile {WRONLY APPEND}]
	    
	    # Save out data information 
	    puts $fileid "\n# BioImage Session\n"
	    puts $fileid "set app_version 1.0"

	    save_class_variables $fileid 
	    
	    close $fileid
	    
	    global NetworkChanged
	    set NetworkChanged 0
	} 
    }

    #########################
    ### save_class_variables
    #########################
    # Save out all of the class variables 
    method save_class_variables { fileid} {
	puts $fileid "\n# Class Variables\n"
	foreach v [info variable] {
	    set var [get_class_variable_name $v]
	    if {$var != "this" && $var != "filters"} {
		puts $fileid "app set_saved_class_var $var \{[set $var]\}"
	    }
	}
	
	# print out arrays
	for {set i 0} {$i < $num_filters} {incr i} {
	    if {[info exists filters($i)]} {
		puts $fileid "app set_saved_class_var filters($i) \{[set filters($i)]\}"
	    }
	}

        # save globals
        global eye
        puts $fileid "global eye"
        puts $fileid "set eye \{[set eye]\}"

        global show_guidelines
        puts $fileid "global show_guidelines"
        puts $fileid "set show_guidelines \{[set show_guidelines]\}"


        global top
        puts $fileid "global top"
        puts $fileid "set top \{[set top]\}"

        global front
        puts $fileid "global front"
        puts $fileid "set front \{[set front]\}"

        global side
        puts $fileid "global side"
        puts $fileid "set side \{[set side]\}"

        global show_plane_x
        puts $fileid "global show_plane_x"
        puts $fileid "set show_plane_x \{[set show_plane_x]\}"

        global show_plane_y
        puts $fileid "global show_plane_y"
        puts $fileid "set show_plane_y \{[set show_plane_y]\}"

        global show_plane_z
        puts $fileid "global show_plane_z"
        puts $fileid "set show_plane_z \{[set show_plane_z]\}"

        global show_MIP_x
        puts $fileid "global show_MIP_x"
        puts $fileid "set show_MIP_x \{[set show_MIP_x]\}"

        global show_MIP_y
        puts $fileid "global show_MIP_y"
        puts $fileid "set show_MIP_y \{[set show_MIP_y]\}"

        global show_MIP_z
        puts $fileid "global show_MIP_z"
        puts $fileid "set show_MIP_z \{[set show_MIP_z]\}"

        global filter2Dtextures
        puts $fileid "global filter2Dtextures"
        puts $fileid "set filter2Dtextures \{[set filter2Dtextures]\}"

        global planes_mapType
        puts $fileid "global planes_mapType"
        puts $fileid "set planes_mapType \{[set planes_mapType]\}"

        global show_vol_ren
        puts $fileid "global show_vol_ren"
        puts $fileid "set show_vol_ren \{[set show_vol_ren]\}"

        global link_winlevel
        puts $fileid "global link_winlevel"
        puts $fileid "set link_winlevel \{[set link_winlevel]\}"

        global vol_width
        puts $fileid "global vol_width"
        puts $fileid "set vol_width \{[set vol_width]\}"

        global vol_level
        puts $fileid "global vol_level"
        puts $fileid "set vol_level \{[set vol_level]\}"

	puts $fileid "app set_saved_class_var loading 1"
    }

    #########################
    ### save_disabled_connections
    #########################
    # Save out the call to disable all modules connections
    # that are currently disabled
    method save_disabled_connections { fileid } {
	global mods Disabled
	
	puts $fileid "\n# Disabled Module Connections\n"
	
	# Check the connections between the ChooseField-X, ChooseField-Y,
	# or ChooseField-Z and the GatherPoints module

	set name "$mods(ChooseField-X)_p0_to_$mods(GatherPoints)_p0"
	if {[info exists Disabled($name)] && $Disabled($name)} {
	    puts $fileid "disableConnection \"\$mods(ChooseField-X) 0 \$mods(GatherPoints) 0\""
	}

	set name "$mods(ChooseField-Y)_p0_to_$mods(GatherPoints)_p1"
	if {[info exists Disabled($name)] && $Disabled($name)} {
	    puts $fileid "disableConnection \"\$mods(ChooseField-Y) 0 \$mods(GatherPoints) 1\""
	}

	set name "$mods(ChooseField-Z)_p0_to_$mods(GatherPoints)_p2"
	if {[info exists Disabled($name)] && $Disabled($name)} {
	    puts $fileid "disableConnection \"\$mods(ChooseField-Z) 0 \$mods(GatherPoints) 2\""
	}	
    }

    ###########################
    ### load_session
    ###########################
    # Load a saved session of BioTensor.  After sourcing the file,
    # reset some of the state (attached, indicate) and configure
    # the tabs and guis. This method also sets the loading to be 
    # true so that when executing, the progress labels don't get
    # all messed up.
    method load_session {} {

	set types {
	    {{App Settings} {.ses} }
	    {{Other} { * }}
	}
	
        set saveFile [tk_getOpenFile -filetypes $types]

	if {$saveFile != ""} {
	    load_session_data
	}
   }


   method load_session_data {} {
       # Clear all modules
       ClearCanvas 0
       
       # configure title
       wm title .standalone "BioImage - [getFileName $saveFile]" 
       
       # remove all UIs from history
       for {set i 0} {$i < $num_filters} {incr i} {
	   if {[info exists filters($i)]} {
               set tmp_row [lindex $filters($i) $which_row]
               if {$tmp_row != -1 } {
		   destroy $history0.$i
		   destroy $history1.$i
	       }
           }
       }

       # justify scroll region
       $attachedPFr.f.p.sf justify top
       $detachedPFr.f.p.sf justify top

       #destroy 2D viewer windows and control panel
       destroy $win.viewers.cp
       destroy $win.viewers.topbot

       # load new net
       foreach g [info globals] {
	   global $g
       }

       update

       # source at the global level for module settings
       uplevel \#0 source \{$saveFile\}

       # save out ViewSlices and Viewer variables that will be overwritten
       # two modules are destroyed and recreated and reset their state
       
       global $mods(ViewSlices)-axial-viewport0-mode
       global $mods(ViewSlices)-sagittal-viewport0-mode
       global $mods(ViewSlices)-coronal-viewport0-mode
       global $mods(ViewSlices)-axial-viewport0-slice
       global $mods(ViewSlices)-sagittal-viewport0-slice
       global $mods(ViewSlices)-coronal-viewport0-slice
       global $mods(ViewSlices)-axial-viewport0-slab_min
       global $mods(ViewSlices)-sagittal-viewport0-slab_min
       global $mods(ViewSlices)-coronal-viewport0-slab_min
       global $mods(ViewSlices)-axial-viewport0-slab_max
       global $mods(ViewSlices)-sagittal-viewport0-slab_max
       global $mods(ViewSlices)-coronal-viewport0-slab_max
       global $mods(ViewSlices)-axial-viewport0-clut_ww
       global $mods(ViewSlices)-axial-viewport0-clut_wl
       global $mods(ViewSlices)-min $mods(ViewSlices)-max

       set axial_mode [set $mods(ViewSlices)-axial-viewport0-mode]
       set sagittal_mode [set $mods(ViewSlices)-sagittal-viewport0-mode]
       set coronal_mode [set $mods(ViewSlices)-coronal-viewport0-mode]
       set axial_slice [set $mods(ViewSlices)-axial-viewport0-slice]
       set sagittal_slice [set $mods(ViewSlices)-sagittal-viewport0-slice]
       set coronal_slice [set $mods(ViewSlices)-coronal-viewport0-slice]
       set axial_slab_min [set $mods(ViewSlices)-axial-viewport0-slab_min]
       set sagittal_slab_min [set $mods(ViewSlices)-sagittal-viewport0-slab_min]
       set coronal_slab_min [set $mods(ViewSlices)-coronal-viewport0-slab_min]
       set axial_slab_max [set $mods(ViewSlices)-axial-viewport0-slab_max]
       set sagittal_slab_max [set $mods(ViewSlices)-sagittal-viewport0-slab_max]
       set coronal_slab_max [set $mods(ViewSlices)-coronal-viewport0-slab_max]
       set ww [set $mods(ViewSlices)-axial-viewport0-clut_ww]
       set wl [set $mods(ViewSlices)-axial-viewport0-clut_wl]

       global $mods(Viewer)-ViewWindow_0-view-eyep-x 
       global $mods(Viewer)-ViewWindow_0-view-eyep-y 
       global $mods(Viewer)-ViewWindow_0-view-eyep-z 
       global $mods(Viewer)-ViewWindow_0-view-lookat-x 
       global $mods(Viewer)-ViewWindow_0-view-lookat-y 
       global $mods(Viewer)-ViewWindow_0-view-lookat-z 
       global $mods(Viewer)-ViewWindow_0-view-up-x 
       global $mods(Viewer)-ViewWindow_0-view-up-y 
       global $mods(Viewer)-ViewWindow_0-view-up-z 
       global $mods(Viewer)-ViewWindow_0-view-fov 

       set eyepx [set $mods(Viewer)-ViewWindow_0-view-eyep-x] 
       set eyepy [set $mods(Viewer)-ViewWindow_0-view-eyep-y] 
       set eyepz [set $mods(Viewer)-ViewWindow_0-view-eyep-z] 
       set lookx [set $mods(Viewer)-ViewWindow_0-view-lookat-x]
       set looky [set $mods(Viewer)-ViewWindow_0-view-lookat-y]
       set lookz [set $mods(Viewer)-ViewWindow_0-view-lookat-z]
       set upx [set $mods(Viewer)-ViewWindow_0-view-up-x]
       set upy [set $mods(Viewer)-ViewWindow_0-view-up-y]
       set upz [set $mods(Viewer)-ViewWindow_0-view-up-z]
       set fov [set $mods(Viewer)-ViewWindow_0-view-fov]

       set loading_ui 1
       set last_valid 0
       set data_tab $cur_data_tab
		
       # iterate over filters array and create UIs
	    for {set i 0} {$i < $num_filters} {incr i} {
		# only build ui for those with a row
		# value not -1
		set status [lindex $filters($i) $which_row]
                set p $i.f$i
		if {$status != -1} {
		    set t [lindex $filters($i) $filter_type]
		    set v [lindex $filters($i) $visibility]
		    set l [lindex $filters($i) $filter_label]
		    set last_valid $i
		    
		    if {[string equal $t "load"]} {
			set f [add_Load_UI $history0 $status $i]
                        $this add_insert_bar $f $i
			set f [add_Load_UI $history1 $status $i]
                        $this add_insert_bar $f $i
			if {$v == 0} {
			    set filters($i) [lreplace $filters($i) $visibility $visibility 1]
			    $this change_visibility $i
			}
		    } elseif {[string equal $t "resample"]} {
			set f [add_Resample_UI $history0 $status $i]
                        $this add_insert_bar $f $i
			set f [add_Resample_UI $history1 $status $i]
                        $this add_insert_bar $f $i
			$this update_kernel $i
			if {$v == 0} {
			    set filters($i) [lreplace $filters($i) $visibility $visibility 1]
			    $this change_visibility $i
			}
		    } elseif {[string equal $t "crop"]} {
			set f [add_Crop_UI $history0 $status $i]
                        $this add_insert_bar $f $i
			set f [add_Crop_UI $history1 $status $i]
                        $this add_insert_bar $f $i
			if {$v == 0} {
			    set filters($i) [lreplace $filters($i) $visibility $visibility 1]
			    $this change_visibility $i
			}
		    } elseif {[string equal $t "cmedian"]} {
			set f [add_Cmedian_UI $history0 $status $i]
                        $this add_insert_bar $f $i
			set f [add_Cmedian_UI $history1 $status $i]
                        $this add_insert_bar $f $i
			if {$v == 0} {
			    set filters($i) [lreplace $filters($i) $visibility $visibility 1]
			    $this change_visibility $i
			}
		    } elseif {[string equal $t "histo"]} {
			set f [add_Histo_UI $history0 $status $i]
                        $this add_insert_bar $f $i
			set f [add_Histo_UI $history1 $status $i]
                        $this add_insert_bar $f $i
			if {$v == 0} {
			    set filters($i) [lreplace $filters($i) $visibility $visibility 1]
			    $this change_visibility $i
			}
		    } else {
			puts "Error: Unknown filter type - $t"
		    }

                    # fix label
		    $history0.$p.childsite.expand.l configure -text $l
		    $history1.$p.childsite.expand.l configure -text $l

		    $history0.$p configure -background grey75 -foreground black -borderwidth 2
		    $history1.$p configure -background grey75 -foreground black -borderwidth 2
            }
	}


        set loading_ui 0

        global eye
        $this change_eye $eye

 	# set a few variables that need to be reset
 	set indicate 0
 	set cycle 0
 	set IsPAttached 1
 	set IsVAttached 1
 	set executing_modules 0

 	$indicatorL0 configure -text "Press Update to run to save point..."
 	$indicatorL1 configure -text "Press Update to run to save point..."

        # update components using globals
        $this update_orientations
        $this update_planes_color_by
        $this update_planes_threshold_slider_min_max [set $mods(ViewSlices)-min] [set $mods(ViewSlices)-max]
        $mods(ViewSlices)-c background_thresh
        $this update_window_level_scales 1 2 3
        $this change_volume_window_width_and_level -1
        $this toggle_show_guidelines

        # bring proper tabs forward
        set cur_data_tab $data_tab
        $attachedPFr.f.p.sf.lwchildsite.clipper.canvas.sfchildsite.0.f0.childsite.ui.tnb view $cur_data_tab
        $detachedPFr.f.p.sf.lwchildsite.clipper.canvas.sfchildsite.0.f0.childsite.ui.tnb view $cur_data_tab
        $this change_vis_frame $c_vis_tab

        # rebuild the viewer windows
        $this build_viewers $mods(Viewer) $mods(ViewSlices)

        # configure slice/mip sliders
        global slice_frame
        foreach axis "sagittal coronal axial" {
            $slice_frame($axis).modes.slider.slice.s configure -from 0 -to [set $axis-size]
            $slice_frame($axis).modes.slider.slab.s configure -from 0 -to [set $axis-size]
        }

        # reset saved ViewSlices variables
        set $mods(ViewSlices)-axial-viewport0-mode $axial_mode
        set $mods(ViewSlices)-axial-viewport0-slice $axial_slice
        set $mods(ViewSlices)-axial-viewport0-slab_min $axial_slab_min
        set $mods(ViewSlices)-axial-viewport0-slab_max $axial_slab_max
        set $mods(ViewSlices)-axial-viewport0-clut_ww $ww
        set $mods(ViewSlices)-axial-viewport0-clut_wl $wl

        set $mods(ViewSlices)-sagittal-viewport0-mode $sagittal_mode
        set $mods(ViewSlices)-sagittal-viewport0-slice $sagittal_slice
        set $mods(ViewSlices)-sagittal-viewport0-slab_min $sagittal_slab_min
        set $mods(ViewSlices)-sagittal-viewport0-slab_max $sagittal_slab_max

        set $mods(ViewSlices)-coronal-viewport0-mode $coronal_mode
        set $mods(ViewSlices)-coronal-viewport0-slice $coronal_slice
        set $mods(ViewSlices)-coronal-viewport0-slab_min $coronal_slab_min
        set $mods(ViewSlices)-coronal-viewport0-slab_max $coronal_slab_max

        # make calls to set up ViewSlices settings properly
        $this update_ViewSlices_mode axial
        $this update_ViewSlices_mode coronal
        $this update_ViewSlices_mode sagittal

        # set viewer settings
       set $mods(Viewer)-ViewWindow_0-view-eyep-x $eyepx
       set $mods(Viewer)-ViewWindow_0-view-eyep-y $eyepy
       set $mods(Viewer)-ViewWindow_0-view-eyep-z $eyepz
       set $mods(Viewer)-ViewWindow_0-view-lookat-x $lookx
       set $mods(Viewer)-ViewWindow_0-view-lookat-y $looky
       set $mods(Viewer)-ViewWindow_0-view-lookat-z $lookz
       set $mods(Viewer)-ViewWindow_0-view-up-x $upx
       set $mods(Viewer)-ViewWindow_0-view-up-y $upy
       set $mods(Viewer)-ViewWindow_0-view-up-z $upz
       set $mods(Viewer)-ViewWindow_0-view-fov $fov

       set has_autoviewed 1
       set 2D_fixed 1
    }	


    #########################
    ### toggle_show_plane_n
    ##########################
    # Methods to turn on/off planes
    method toggle_show_plane_x {} {
	$this check_crop
	global mods show_plane_x
	if {$show_plane_x == 1} {
	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-Slice0 (1)\}\" 1; $mods(Viewer)-ViewWindow_0-c redraw"
	} else {
	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-Slice0 (1)\}\" 0; $mods(Viewer)-ViewWindow_0-c redraw"
	}
    }

    method toggle_show_plane_y {} {
	$this check_crop
	global mods show_plane_y
	if {$show_plane_y == 1} {
	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-Slice1 (1)\}\" 1; $mods(Viewer)-ViewWindow_0-c redraw"
	} else {
	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-Slice1 (1)\}\" 0; $mods(Viewer)-ViewWindow_0-c redraw"
	}
    }

    method toggle_show_plane_z {} {
	$this check_crop
	global mods show_plane_z
	if {$show_plane_z == 1} {
	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-Slice2 (1)\}\" 1; $mods(Viewer)-ViewWindow_0-c redraw"
	} else {
	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-Slice2 (1)\}\" 0; $mods(Viewer)-ViewWindow_0-c redraw"
	}
    }


    #########################
    ### toggle_show_MIP_n
    ##########################
    # Methods to turn on/off MIP
    method toggle_show_MIP_x {} {
	$this check_crop
	global mods show_MIP_x
	if {$show_MIP_x == 1} {
 	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-MIP Slice0 (1)\}\" 1; $mods(Viewer)-ViewWindow_0-c redraw"
  	} else {
 	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-MIP Slice0 (1)\}\" 0; $mods(Viewer)-ViewWindow_0-c redraw"
  	}
    }

    method toggle_show_MIP_y {} {
	$this check_crop
	global mods show_MIP_y
	if {$show_MIP_y == 1} {
	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-MIP Slice1 (1)\}\" 1; $mods(Viewer)-ViewWindow_0-c redraw"
  		} else {
 	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-MIP Slice1 (1)\}\" 0; $mods(Viewer)-ViewWindow_0-c redraw"
  	}
    }

    method toggle_show_MIP_z {} {
	$this check_crop
	global mods show_MIP_z
	if {$show_MIP_z == 1} {
	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-MIP Slice2 (1)\}\" 1; $mods(Viewer)-ViewWindow_0-c redraw"
  		} else {
 	    after 100 "uplevel \#0 set \"\{$mods(Viewer)-ViewWindow_0-MIP Slice2 (1)\}\" 0; $mods(Viewer)-ViewWindow_0-c redraw"
  	}
    }

    method update_planes_threshold_slider_min_max { min max } {
	$attachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page1.cs.thresh.s configure -from $min -to $max
	$detachedVFr.f.vis.childsite.tnb.canvas.notebook.cs.page1.cs.thresh.s configure -from $min -to $max
    }

   method toggle_filter2Dtextures {} {
       global filter2Dtextures mods slice_frame
       upvar \#0 $mods(ViewSlices)-texture_filter filter
       set filter $filter2Dtextures

       $mods(ViewSlices)-c texture_rebind $slice_frame(axial).bd.axial
       $mods(ViewSlices)-c texture_rebind $slice_frame(sagittal).bd.sagittal
       $mods(ViewSlices)-c texture_rebind $slice_frame(coronal).bd.coronal


   }

    method toggle_show_guidelines {} {

  	 $this check_crop

         global mods show_guidelines
         global $mods(ViewSlices)-axial-viewport0-show_guidelines
         global $mods(ViewSlices)-sagittal-viewport0-show_guidelines
         global $mods(ViewSlices)-coronal-viewport0-show_guidelines

         set $mods(ViewSlices)-axial-viewport0-show_guidelines $show_guidelines
         set $mods(ViewSlices)-sagittal-viewport0-show_guidelines $show_guidelines
         set $mods(ViewSlices)-coronal-viewport0-show_guidelines $show_guidelines
  }

    method update_planes_color_by {} {
        $this check_crop

        global planes_mapType
        set GenStandard [lindex [lindex $filters(0) $modules] 26]
        global [set GenStandard]-mapType

        set [set GenStandard]-mapType $planes_mapType
        if {!$loading && $has_executed == 1} {
	    [set GenStandard]-c needexecute
	}
    }

    method toggle_show_vol_ren {} {
        $this check_crop
	global mods show_vol_ren 

	set VolumeVisualizer [lindex [lindex $filters(0) $modules] 14]
	set NodeGradient [lindex [lindex $filters(0) $modules] 16]
	set FieldToNrrd [lindex [lindex $filters(0) $modules] 17]
	set UnuQuantize [lindex [lindex $filters(0) $modules] 7]
	set UnuJhisto [lindex [lindex $filters(0) $modules] 21]
        set EditColorMap2D [lindex [lindex $filters(0) $modules] 13]
        set NrrdTextureBuilder [lindex [lindex $filters(0) $modules] 11]
        set UnuProject [lindex [lindex $filters(0) $modules] 12]

        if {$show_vol_ren == 1} {
	    disableModule [set VolumeVisualizer] 0
	    disableModule [set NodeGradient] 0
	    disableModule [set FieldToNrrd] 0
	    disableModule [set UnuQuantize] 0
	    disableModule [set UnuJhisto] 0
	    disableModule [set EditColorMap2D] 0
	    disableModule [set NrrdTextureBuilder] 0
	    disableModule [set UnuProject] 0

            change_indicator_labels "Volume Rendering..."

            [set NodeGradient]-c needexecute
        } else {
	    disableModule [set VolumeVisualizer] 1
	    disableModule [set NodeGradient] 1
	    disableModule [set FieldToNrrd] 1
	    disableModule [set UnuQuantize] 1
	    disableModule [set UnuJhisto] 1
	    disableModule [set EditColorMap2D] 1
	    disableModule [set NrrdTextureBuilder] 1
	    disableModule [set UnuProject] 1
        }
    }

    method update_ViewSlices_mode { axis args } {
	global mods slice_frame
        upvar \#0 $mods(ViewSlices)-$axis-viewport0-mode mode

        set w $slice_frame($axis)
        # forget and repack appropriate widget
	if {$mode == 0} {
	    # Slice mode
            pack forget $w.modes.slider.slab
            pack $w.modes.slider.slice -side top -anchor n -expand 1 -fill x
	} elseif {$mode == 1} {
	    # Slab mode
    	    pack forget $w.modes.slider.slice
            pack $w.modes.slider.slab -side top -anchor n -expand 1 -fill x
	} else {
	    # Full MIP mode
            $mods(ViewSlices)-c rebind $w.$axis
            pack forget $w.modes.slider.slice
  	    pack forget $w.modes.slider.slab
	}
        $mods(ViewSlices)-c rebind $slice_frame($axis).bd.$axis
        $mods(ViewSlices)-c redrawall
    }

    method set_saved_class_var {var val} {
	set $var $val
    }

    method set_viewer_position {} {
	global mods
	
	global $mods(Viewer)-ViewWindow_0-view-eyep-x
	global $mods(Viewer)-ViewWindow_0-view-eyep-y
	global $mods(Viewer)-ViewWindow_0-view-eyep-z
   	set $mods(Viewer)-ViewWindow_0-view-eyep-x {560.899236544}
        set $mods(Viewer)-ViewWindow_0-view-eyep-y {356.239586973}
        set $mods(Viewer)-ViewWindow_0-view-eyep-z {178.810334192}

	global $mods(Viewer)-ViewWindow_0-view-lookat-x
	global $mods(Viewer)-ViewWindow_0-view-lookat-y
	global $mods(Viewer)-ViewWindow_0-view-lookat-z
        set $mods(Viewer)-ViewWindow_0-view-lookat-x {51.5}
        set $mods(Viewer)-ViewWindow_0-view-lookat-y {47.0}
        set $mods(Viewer)-ViewWindow_0-view-lookat-z {80.5}

	global $mods(Viewer)-ViewWindow_0-view-up-x
	global $mods(Viewer)-ViewWindow_0-view-up-y
	global $mods(Viewer)-ViewWindow_0-view-up-z
        set $mods(Viewer)-ViewWindow_0-view-up-x {-0.181561715965}
        set $mods(Viewer)-ViewWindow_0-view-up-y {0.0242295849764}
        set $mods(Viewer)-ViewWindow_0-view-up-z {0.983081009128}

	global $mods(Viewer)-ViewWindow_0-view-fov
        set $mods(Viewer)-ViewWindow_0-view-fov {20.0}
    }
    

    method maybe_autoview { args } {
	global mods
	if {$has_autoviewed == 0} {
          set has_autoviewed 1
          $mods(Viewer)-ViewWindow_0-c autoview
	}
    }



    # Application placing and size
    variable notebook_width
    variable notebook_height


    # Data Selection
    variable vis_frame_tab1
    variable vis_frame_tab2

    variable filters
    variable num_filters
    variable loading_ui

    variable history0
    variable history1

    variable dimension

    variable scolor

    # filter indexes
    variable modules
    variable input
    variable output
    variable prev_index
    variable next_index
    variable choose_port
    # set to -1 when deleted
    variable which_row  
    variable visibility
    variable filter_label
    variable filter_type

    variable load_choose_input
    variable load_nrrd
    variable load_dicom
    variable load_analyze
    variable load_field
    variable load_choose_vis
    variable load_info

    variable grid_rows

    variable label_width

    variable 0_samples
    variable 1_samples
    variable 2_samples

    variable has_autoviewed
    variable has_executed

    variable data_dir
    variable 2D_fixed
    variable ViewSlices_executed_on_error
    variable current_crop
    variable turn_off_crop
    variable updating_crop_ui
    variable needs_update
    variable enter_crop

    variable cur_data_tab
    variable c_vis_tab

    variable axial-size
    variable sagittal-size
    variable coronal-size
}


setProgressText "Building BioImage Window..."

BioImageApp app
app build_app $DATADIR

hideProgress


### Bind shortcuts - Must be after instantiation of App
bind all <Control-s> {
    app save_session
}

bind all <Control-o> {
    app load_session
}

bind all <Control-q> {
    app exit_app
}

bind all <Control-v> {
    global mods
    $mods(Viewer)-ViewWindow_0-c autoview
}
