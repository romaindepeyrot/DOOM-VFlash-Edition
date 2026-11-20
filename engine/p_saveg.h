// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	Savegame I/O, archiving, persistence.
//
//-----------------------------------------------------------------------------


#ifndef __P_SAVEG__
#define __P_SAVEG__

#include "doomtype.h"

#ifdef __GNUG__
#pragma interface
#endif


// Persistent storage/archiving.
// These are the load / save game routines.
boolean P_ArchivePlayers (byte *end);
void P_UnArchivePlayers (void);
boolean P_ArchiveWorld (byte *end);
void P_UnArchiveWorld (void);
boolean P_ArchiveThinkers (byte *end);
void P_UnArchiveThinkers (void);
boolean P_ArchiveSpecials (byte *end);
void P_UnArchiveSpecials (void);

extern byte*		save_p; 


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
