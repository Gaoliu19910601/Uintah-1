
/*
 *  Modules.cc: Build the database of Modules...
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   March 1994
 *
 *  Copyright (C) 1994 SCI Group
 */

#include <ModuleList.h>
#include <Classlib/String.h>

void ModuleList::initialize_list()
{
    Module* make_FieldReader();
    ModuleList::insert("FieldReader", make_FieldReader);
    Module* make_IsoSurface();
    ModuleList::insert("IsoSurface", make_IsoSurface);
    Module* make_Salmon();
    ModuleList::insert("Salmon", make_Salmon);
    Module* make_SoundFilter();
    ModuleList::insert("SoundFilter", make_SoundFilter);
    Module* make_SoundInput();
    ModuleList::insert("SoundInput", make_SoundInput);
    Module* make_SoundMixer();
    ModuleList::insert("SoundMixer", make_SoundMixer);
    Module* make_SoundOutput();
    ModuleList::insert("SoundOutput", make_SoundOutput);
    Module* make_SoundReader();
    ModuleList::insert("SoundReader", make_SoundReader);
}
