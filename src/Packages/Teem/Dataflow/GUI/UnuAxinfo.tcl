#  The contents of this file are subject to the University of Utah Public
#  License (the "License"); you may not use this file except in compliance
#  with the License.
#  
#  Software distributed under the License is distributed on an "AS IS"
#  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
#  License for the specific language governing rights and limitations under
#  the License.
#  
#  The Original Source Code is SCIRun, released March 12, 2001.
#  
#  The Original Source Code was developed by the University of Utah.
#  Portions created by UNIVERSITY are Copyright (C) 2001, 1994
#  University of Utah. All Rights Reserved.
#  
#    File   : UnuAxinfo.tcl
#    Author : Darby Van Uitert
#    Date   : January 2004


itcl_class Teem_Unu_UnuAxinfo {
    inherit Module
    constructor {config} {
        set name UnuAxinfo
        set_defaults
    }
    
    method set_defaults {} {
	# the width of the first column of the data display
	global $this-firstwidth
	global $this-type
	global $this-dimension
	global $this-active_tab
	global $this-label0
	global $this-kind0
	global $this-center0
	global $this-size0
	global $this-spacing0
	global $this-min0
	global $this-max0
	global $this-reset
	global $this-initialized
	
	set $this-dimension 0
	set $this-type "---"
	
	# these won't be saved 
	set $this-firstwidth 12
	set $this-active_tab "Axis 0"
	set $this-label0 "---"
	set $this-kind0 "nrrdKindUnknown"
	set $this-center0 "---"
	set $this-size0 "---"
	set $this-spacing0 "---"
	set $this-min0 "---"
	set $this-max0 "---"
	set $this-reset 0
	set $this-initialized 0
    }
    
    method set_active_tab {act} {
	global $this-active_tab
	#puts stdout $act
	set $this-active_tab $act
    }
    
    method switch_to_active_tab {name1 name2 op} {
	#puts stdout "switching"
	set window .ui[modname]
	if {[winfo exists $window]} {
	    set axis_frame [$w.att.axis_info childsite]
	    $axis_frame.tabs view [set $this-active_tab]
	}
    }
    
    #     method fill_tuple_tab {} {
    #         set w .ui[modname]
    
    # 	if {![winfo exists $w]} {
    #             return
    #         }
    # 	set att [$w.att childsite]
    # 	# tuple tab is always first
    # 	set tuple [$att.tabs childsite 0]
    # 	#parse the label axis string to gather all the info out of it.
    # 	set l [regexp -inline -all -- {([\w-]+):(\w+),?} [set $this-label0]]
    
    # 	set last [$tuple.listbox index end]
    # 	$tuple.listbox delete 0 $last
    # 	$tuple.listbox insert 0 "Name (Type)"
    
    # 	if {[llength $l]} {
    # 	    set ind 1
    # 	    foreach {match axname type} $l {
    # 		$tuple.listbox insert $ind "$axname ($type)"
    # 		incr ind
    # 	    }
    # 	} else {
    # 	    $tuple.listbox insert 1 "unknown tuple axis label format"
    # 	    $tuple.listbox insert 2 [set $this-label0]
    # 	}
    #     }
    
    method clear_axes {} {
	set w .ui[modname]
        if {[winfo exists $w]} {
	    
	    for {set i 0} {$i < [set $this-dimension]} {incr i} {
		
		unset $this-label$i
		unset $this-kind$i
		unset $this-center$i
		unset $this-size$i
		unset $this-spacing$i
		unset $this-min$i
		unset $this-max$i
	    }
	    set $this-reset 1
	    
	    delete_tabs
	}
    }
    
    method delete_tabs {} {
	set w .ui[modname]
        if {[winfo exists $w.att]} {
	    set af [$w.att childsite]
	    set l [$af.tabs childsite]
	    if { [llength $l] > 1 } { 
		$af.tabs delete 1 end
	    }
	    set_active_tab "Axis 0"
	    $af.tabs view [set $this-active_tab]
	    
	    # Clear Axis 0 and dimension
	    set $this-dimension 0
	    set $this-label0 "---"
	    set $this-kind0 "nrrdKindUnknown"
	    set $this-center0 "---"
	    set $this-size0 "---"
	    set $this-spacing0 "---"
	    set $this-min0 "---"
	    set $this-max0 "---"
	}
    }
    
    method init_axes {} {
	for {set i 0} {$i < [set $this-dimension]} {incr i} {
	    
	    if { [catch { set t [set $this-label$i] } ] } {
		set $this-label$i ""
	    }
	    if { [catch { set t [set $this-kind$i] } ] } {
		set $this-kind$i ""
	    }
	    if { [catch { set t [set $this-center$i]}] } {
		set $this-center$i ""
	    }
	    if { [catch { set t [set $this-size$i]}] } {
		set $this-size$i 0
	    }
	    if { [catch { set t [set $this-spacing$i]}] } {
		set $this-spacing$i 0
	    }
	    if { [catch { set t [set $this-min$i]}] } {
		set $this-min$i 0
	    }
	    if { [catch { set t [set $this-max$i]}] } {
		set $this-max$i 0
	    }
	}
	# fill_tuple_tab
	add_tabs
    }
    
    method add_tabs {} {
	set w .ui[modname]
        if {[winfo exists $w]} {
	    
	    set af [$w.att childsite]
	    
	    if {[set $this-initialized]} {
		for {set i 1} {$i < [set $this-dimension]} {incr i} {
		    set t [$af.tabs add -label "Axis $i" \
			       -command "$this set_active_tab \"Axis $i\""]
		    makelabelentry $t.l "Label" $this-label$i
		    Tooltip $t.l "Change the label\nfor Axis $i."
		    make_kind_optionmenu $t.k $this-kind$i $i
		    labelpair $t.c "Center" $this-center$i
		    labelpair $t.sz "Size" $this-size$i
		    makelabelentry $t.sp "Spacing" $this-spacing$i
		    Tooltip $t.sp "Change spacing between\nsamples along Axis $i. This\nshould be expressed as a\ndouble."
		    makelabelentry $t.mn "Min" $this-min$i
		    Tooltip $t.mn "Change the minimum value\nalong Axis $i. This should\nbe expressed as a double."
		    makelabelentry $t.mx "Max" $this-max$i
		    Tooltip $t.mx "Change the maximum value\nalong Axis $i. This should\nbe expressed as a double."
		    
		    pack $t.l $t.c $t.sz  $t.sp $t.mn $t.mx  -side top
		    pack $t -side top -fill both -expand 1
		}
	    } else {
		# Build Axis 0 tab as empty
		set t [$af.tabs add -label "Axis 0" \
			   -command "$this set_active_tab \"Axis 0\""]
		
		makelabelentry $t.l "Label" $this-label0
		Tooltip $t.l "Change the label\nfor Axis 0."
		make_kind_optionmenu $t.k $this-kind0 0
		labelpair $t.c "Center" $this-center0
		labelpair $t.sz "Size" $this-size0
		makelabelentry $t.sp "Spacing" $this-spacing0
		Tooltip $t.sp "Change spacing between\nsamples along Axis 0. This\nshould be expressed as a\ndouble."
		makelabelentry $t.mn "Min" $this-min0
		Tooltip $t.mn "Change the minimum value\nalong Axis 0. This should\nbe expressed as a double."
		makelabelentry $t.mx "Max" $this-max0
		Tooltip $t.mx "Change the maximum value\nalong Axis 0. This should\nbe expressed as a double."
		
		pack $t.l $t.c $t.sz  $t.sp $t.mn $t.mx  -side top
		pack $t -side top -fill both -expand 1	
		set $this-initialized 1
		
	    }
	}
    }
    
    #     method add_tuple_tab {af} {
    
    # 	set tuple [$af.tabs add -label "Tuple Axis" \
	# 		       -command "$this set_active_tab \"Tuple Axis\""]
    
    # 	iwidgets::scrolledlistbox $tuple.listbox -vscrollmode static \
	# 	    -hscrollmode dynamic -scrollmargin 3 -height 60
    
    # 	 fill_tuple_tab
    # 	 pack $tuple.listbox -side top -fill both -expand 1
    #     }
    
    method ui {} {
        set w .ui[modname]
        if {[winfo exists $w]} {
            return
        }
        toplevel $w
	
	# notebook for the axis specific info.
	iwidgets::labeledframe $w.att -labelpos nw \
	    -labeltext "Nrrd Attributes" 
	
	pack $w.att -expand y -fill both
	set att [$w.att childsite]
	
	labelpair $att.type "C Type" $this-type
	labelpair $att.dim "Dimension" $this-dimension
	
	# notebook for the axis specific info.
	iwidgets::tabnotebook  $att.tabs -height 200 -width 325 \
	    -raiseselect true 
	
	# add_tuple_tab $att
	init_axes
	
	# view the active tab
	$att.tabs view [set $this-active_tab]	
	$att.tabs configure -tabpos "n"
	
	pack $att.type $att.dim -side top 
	pack $att.tabs -side top -fill x -expand yes
	
	makeSciButtonPanel $w $w $this
	moveToCursor $w
    }
    
    
    method labelpair { win text1 text2 } {
	global $text2
	
	frame $win 
	pack $win -side top -padx 5 -pady 1
	label $win.l1 -text $text1 -width [set $this-firstwidth] \
	    -anchor w -just left
	label $win.colon  -text ":" -width 2 -anchor w -just left 
	label $win.l2 -textvar $text2 -width 40 -anchor w -just left \
	    -fore darkred
	pack $win.l1 $win.colon $win.l2 -side left
    } 
    
    method makelabelentry { win l var} {
	global $var
	
	frame $win
	pack $win -side left -padx 5 -pady 1 -fill x
	label $win.l -text "$l" -width [set $this-firstwidth] \
	    -anchor w -just left
	label $win.colon -text ":" -width 2 -anchor w -just left
	entry $win.e -textvar $var -width 10 \
	    -foreground darkred
	
	pack $win.l $win.colon $win.e -side left
    } 
    
    method make_kind_optionmenu { win var i} {
	global $var
	
	iwidgets::optionmenu $win -labeltext "Kind" \
	    -labelpos w -command "$this update_kind $win $var"
	$win insert end nrrdKindUnknown nrrdKindDomain nrrdKindScalar \
	    nrrdKind3Color nrrdKind3Vector nrrdKind3Normal \
	    nrrdKind3DSymTensor nrrdKind3DMaskedSymTensor nrrdKind3DTensor
	if {[info exists $this-kind$i]} {
	    $win select [set $this-kind$i]
	} else {
	    $win select nrrdKindUnknown
	}
	pack $win -side top -anchor nw -padx 3 -pady 3

	trace variable $var w "$this update_kind_menu $i"
    }
    
    method update_kind {w var} {
	set which [$w get]
	
	set $var $which
    }

    method update_kind_menu {i name1 name2 op} {
	set window .ui[modname]
	
	set j [expr $i + 1]
	if {[winfo exists $window]} {
	    set op $window.att.childsite.tabs.canvas.notebook.cs.page$j.cs.k 
	    $op select [set $this-kind$i]
	}
    }


}
