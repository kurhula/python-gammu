/* (c) 2003 by Marcin Wiacek */
/* www.irda.org OBEX specs 1.3 */

/* Module connects to F9EC7BC4-953c-11d2-984E-525400DC9E09 UUID and in the
 * future there will required implementing reconnecting. See "ifdef xxxx"
 */

#include <string.h>
#include <time.h>

#include "../../misc/coding/coding.h"
#include "../../gsmcomon.h"
#include "../../gsmstate.h"
#include "../../service/gsmmisc.h"
#include "../../protocol/obex/obex.h"
#include "obexgen-functions.h"

#ifdef GSM_ENABLE_OBEXGEN

GSM_Error OBEXGEN_GetNextFileFolder(GSM_StateMachine *s, GSM_File *File, bool start);

static void OBEXGEN_FindNextDir(unsigned char *Path, int *Pos, unsigned char *Return)
{
	int Retlen = 0;

	while(1) {
		if (Path[(*Pos)*2] == 0 && Path[(*Pos)*2+1] == 0) break;
		if (Path[(*Pos)*2] == 0 && Path[(*Pos)*2+1] == '/') {
			(*Pos)++;
			break;
		}
		Return[Retlen*2]     = Path[(*Pos)*2];
		Return[Retlen*2+1]   = Path[(*Pos)*2+1];
		(*Pos)++;
		Retlen++;
	}
	Return[Retlen*2]     = 0;
	Return[Retlen*2+1]   = 0;
}

static GSM_Error OBEXGEN_ReplyConnect(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	switch (msg.Type) {
	case 0xA0:
		smprintf(s,"Connected/disconnected OK\n");
		if (msg.Length != 0) {
			s->Phone.Data.Priv.OBEXGEN.FrameSize = msg.Buffer[2]*256+msg.Buffer[3];
			smprintf(s,"Maximal size of frame is %i 0x%x\n",s->Phone.Data.Priv.OBEXGEN.FrameSize,s->Phone.Data.Priv.OBEXGEN.FrameSize);
		}
		return ERR_NONE;
	case 0xC0:
		smprintf(s, "Wrong request sent to phone!\n");
		return ERR_BUG;
	}
	return ERR_UNKNOWNRESPONSE;
}

GSM_Error OBEXGEN_Disconnect(GSM_StateMachine *s)
{
	smprintf(s, "Disconnecting\n");
	return GSM_WaitFor (s, NULL, 0, 0x81, 2, ID_Initialise);
}

GSM_Error OBEXGEN_Connect(GSM_StateMachine *s, OBEX_Service service)
{
	GSM_Error	error = ERR_NONE;
	int		Current=4;
	unsigned char 	req2[200];
	unsigned char 	req[200] = {
		0x10,			/* Version 1.0 			*/
		0x00,			/* no flags 			*/
		0x04,0x00};		/* 0x2000 max size of packet 	*/

	if (service == s->Phone.Data.Priv.OBEXGEN.Service) return ERR_NONE;

	/* Disconnect from current service, if we were connected */
	if (s->Phone.Data.Priv.OBEXGEN.Service != 0) {
		error = OBEXGEN_Disconnect(s);
		if (error != ERR_NONE) return error;
	}

	switch (service) {
	case OBEX_None:
		smprintf(s, "No service requested\n");
		break;
	case OBEX_IRMC:
		smprintf(s, "IrMC service requested\n");
		/* IrMC Service UUID */
		req2[0] = 'I'; req2[1] = 'R'; req2[2] = 'M';
		req2[3] = 'C'; req2[4] = '-'; req2[5] = 'S';
		req2[6] = 'Y'; req2[7] = 'N'; req2[8] = 'C';

		/* Target block */
		OBEXAddBlock(req, &Current, 0x46, req2, 9);
		break;
	case OBEX_BrowsingFolders:
		smprintf(s, "Folder Browsing service requested\n");
		/* Folder Browsing Service UUID */
		req2[0] = 0xF9; req2[1] = 0xEC; req2[2] = 0x7B;
		req2[3] = 0xC4; req2[4] = 0x95; req2[5] = 0x3C;
		req2[6] = 0x11; req2[7] = 0xD2; req2[8] = 0x98;
		req2[9] = 0x4E; req2[10]= 0x52; req2[11]= 0x54;
		req2[12]= 0x00; req2[13]= 0xDC; req2[14]= 0x9E;
		req2[15]= 0x09;

		/* Target block */
		OBEXAddBlock(req, &Current, 0x46, req2, 16);
		break;
	}

#ifndef xxxx
	//disconnect old service
#else
	if (s->Phone.Data.Priv.OBEXGEN.Service != 0) return ERR_NONE;
#endif

	s->Phone.Data.Priv.OBEXGEN.Service = service;

	smprintf(s, "Connecting\n");
	return GSM_WaitFor (s, req, Current, 0x80, 2, ID_Initialise);
}

/**
 * Initializes OBEX connection in desired mode as defined by config.
 */
GSM_Error OBEXGEN_Initialise(GSM_StateMachine *s)
{
	GSM_Error	error = ERR_NONE;

	s->Phone.Data.Priv.OBEXGEN.Service = 0;

	/**
	 * @todo: In IrMC mode we might read real model from
	 * telecom/devinfo.txt, in browsing mode from obex capability
	 * XML
	 */
	strcpy(s->Phone.Data.Model, "OBEX");

	s->Phone.Data.VerNum 		= 0;
	s->Phone.Data.Version[0] 	= 0;
	s->Phone.Data.Manufacturer[0] 	= 0;

	/* Initialise connection for desired type */
	smprintf(s, "Connected using model %s\n", s->CurrentConfig->Model);
	if (strcmp(s->CurrentConfig->Model, "obex") == 0) {
		error = OBEXGEN_Connect(s,OBEX_BrowsingFolders);
	} else if (strcmp(s->CurrentConfig->Model, "obexirmc") == 0) {
		error = OBEXGEN_Connect(s,OBEX_IRMC);
	} else if (strcmp(s->CurrentConfig->Model, "obexnone") == 0) {
		error = OBEXGEN_Connect(s,OBEX_IRMC);
	}

	return ERR_NONE;
}

static GSM_Error OBEXGEN_ReplyChangePath(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	switch (msg.Type) {
	case 0xA0:
		smprintf(s,"Path set OK\n");
		return ERR_NONE;
	case 0xA1:
		smprintf(s,"Folder created\n");
		return ERR_NONE;
	case 0xC3:
		smprintf(s,"Security error\n");
		return ERR_SECURITYERROR;
	case 0xC4:
		smprintf(s,"File not found\n");
		return ERR_FILENOTEXIST;
	}
	return ERR_UNKNOWNRESPONSE;
}

static GSM_Error OBEXGEN_ChangePath(GSM_StateMachine *s, char *Name, unsigned char Flag1)
{
	unsigned char 	req[400];
	int		Current = 2;

	/* Flags */
	req[0] = Flag1;
	req[1] = 0x00;

	/* Name block */
	if (Name != NULL && UnicodeLength(Name) != 0) {
		OBEXAddBlock(req, &Current, 0x01, Name, UnicodeLength(Name)*2+2);
	} else {
		OBEXAddBlock(req, &Current, 0x01, NULL, 0);
	}

	/* connection ID block */
	req[Current++] = 0xCB; // ID
	req[Current++] = 0x00; req[Current++] = 0x00;
	req[Current++] = 0x00; req[Current++] = 0x01;

	return GSM_WaitFor (s, req, Current, 0x85, 4, ID_SetPath);
}

static GSM_Error OBEXGEN_ReplyAddFilePart(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	switch (msg.Type) {
	case 0x90:
		smprintf(s,"Last part of file added OK\n");
		return ERR_NONE;
	case 0xA0:
		smprintf(s,"Part of file added OK\n");
		return ERR_NONE;
	case 0xC0:
		smprintf(s,"Not understand. Probably not supported\n");
		return ERR_NOTSUPPORTED;
	case 0xC3:
		smprintf(s,"Security error\n");
		return ERR_SECURITYERROR;
	case 0xC4:
		smprintf(s,"File not found\n");
		return ERR_FILENOTEXIST;
	}
	return ERR_UNKNOWNRESPONSE;
}

GSM_Error OBEXGEN_AddFilePart(GSM_StateMachine *s, GSM_File *File, int *Pos, int *Handle)
{
	GSM_Error		error;
	int			j;
	unsigned int		Pos2, Current = 0;
	unsigned char 		req[2000],req2[200];

	s->Phone.Data.File = File;

	if (*Pos == 0) {
		if (!strcmp(DecodeUnicodeString(File->ID_FullName),"")) {
			/**
			 * @todo: This was probably supposed to be used as regullar
			 * send file to phone and let it handle data, does it actually
			 * work?
			 */
			error = OBEXGEN_Connect(s,OBEX_None);
			if (error != ERR_NONE) return error;
		} else {
			if (strcmp(s->CurrentConfig->Model,"seobex")) {
				smprintf(s,"Changing to root\n");
				error = OBEXGEN_ChangePath(s, NULL, 2);
				if (error != ERR_NONE) return error;

				Pos2 = 0;
				do {
					OBEXGEN_FindNextDir(File->ID_FullName, &Pos2, req2);
					smprintf(s,"%s %i %zi\n",DecodeUnicodeString(req2),Pos2,UnicodeLength(File->ID_FullName));
					smprintf(s,"Changing path down\n");
					error=OBEXGEN_ChangePath(s, req2, 2);
					if (error != ERR_NONE) return error;
					if (Pos2 == UnicodeLength(File->ID_FullName)) break;
				} while (1);
			}
		}

		/* Name block */
		OBEXAddBlock(req, &Current, 0x01, File->Name, UnicodeLength(File->Name)*2+2);

		/* File size block */
		req[Current++] = 0xC3; // ID
		req[Current++] = 0;
		req[Current++] = 0;
		req[Current++] = File->Used / 256;
		req[Current++] = File->Used % 256;
	}

	if (s->Phone.Data.Priv.OBEXGEN.Service == OBEX_BrowsingFolders) {
		/* connection ID block */
		req[Current++] = 0xCB; // ID
		req[Current++] = 0x00; req[Current++] = 0x00;
		req[Current++] = 0x00; req[Current++] = 0x01;
	}

	j = s->Phone.Data.Priv.OBEXGEN.FrameSize - Current - 20;
	if (j > 1000) j = 1000;

	if (File->Used - *Pos < j) {
		j = File->Used - *Pos;
		/* End of file body block */
		OBEXAddBlock(req, &Current, 0x49, File->Buffer+(*Pos), j);
		smprintf(s, "Adding file part %i %i\n",*Pos,j);
		*Pos = *Pos + j;
		error = GSM_WaitFor (s, req, Current, 0x82, 4, ID_AddFile);
		if (error != ERR_NONE) return error;
		return ERR_EMPTY;
	} else {
		/* File body block */
		OBEXAddBlock(req, &Current, 0x48, File->Buffer+(*Pos), j);
		smprintf(s, "Adding file part %i %i\n",*Pos,j);
		*Pos = *Pos + j;
		error=GSM_WaitFor (s, req, Current, 0x02, 4, ID_AddFile);
	}
	return error;
}

static GSM_Error OBEXGEN_ReplyGetFilePart(GSM_Protocol_Message msg, GSM_StateMachine *s)
{
	int old,Pos=0;

	switch (msg.Type) {
	case 0xA0:
		smprintf(s,"Last file part received\n");
		s->Phone.Data.Priv.OBEXGEN.FileLastPart = true;
	case 0x90:
		while(1) {
			if (Pos >= msg.Length) break;
			switch (msg.Buffer[Pos]) {
			case 0x48:
			case 0x49:
				smprintf(s,"File part received\n");
				old = s->Phone.Data.File->Used;
				s->Phone.Data.File->Used += msg.Buffer[Pos+1]*256+msg.Buffer[Pos+2]-3;
				smprintf(s,"Length of file part: %i\n",
						msg.Buffer[Pos+1]*256+msg.Buffer[Pos+2]-3);
				s->Phone.Data.File->Buffer = (unsigned char *)realloc(s->Phone.Data.File->Buffer,s->Phone.Data.File->Used);
				memcpy(s->Phone.Data.File->Buffer+old,msg.Buffer+Pos+3,s->Phone.Data.File->Used-old);
				return ERR_NONE;
			case 0xc3:
				/* Length */
				/* FIXME: ignored now */
				Pos += 5;
			case 0xcb:
				/* Skip Connection ID (we ignore this for now) */
				Pos += 5;
				break;
			default:
				Pos+=msg.Buffer[Pos+1]*256+msg.Buffer[Pos+2];
				break;
			}
		}
		return ERR_UNKNOWNRESPONSE;
	case 0xC3:
		smprintf(s,"Security error\n");
		return ERR_SECURITYERROR;
	case 0xC0:
		smprintf(s,"Invalid request\n");
		return ERR_BUG;
	case 0xC4:
		smprintf(s,"File not found\n");
		return ERR_FILENOTEXIST;
	}
	return ERR_UNKNOWNRESPONSE;
}

static GSM_Error OBEXGEN_PrivGetFilePart(GSM_StateMachine *s, GSM_File *File, bool FolderList)
{
	unsigned int 		Current = 0, Pos;
	GSM_Error		error;
	unsigned char 		req[2000], req2[200];

	s->Phone.Data.File 	= File;
	File->ReadOnly 		= false;
	File->Protected 	= false;
	File->Hidden		= false;
	File->System		= false;
	File->ModifiedEmpty	= true;

	if (File->Used == 0x00) {
		if (FolderList) {
			/* Type block */
			strcpy(req2,"x-obex/folder-listing");
			OBEXAddBlock(req, &Current, 0x42, req2, strlen(req2)+1);

			/* Name block */
			if (UnicodeLength(File->Name) == 0x00) {
				OBEXAddBlock(req, &Current, 0x01, NULL, 0);
			} else {
				CopyUnicodeString(req2,File->Name);
				OBEXAddBlock(req, &Current, 0x01, req2, UnicodeLength(req2)*2+2);
			}
		} else {
			File->Folder = false;

			if (File->ID_FullName[0] == 0x00 && File->ID_FullName[1] == 0x00) {
				if (s->Phone.Data.Priv.OBEXGEN.Service == OBEX_BrowsingFolders) {
					/* No file name? Grab OBEX capabilities in browse mode */
					smprintf(s, "No filename requested, grabbing OBEX capabilities as obex-capability.xml\n");
					EncodeUnicode(File->Name, "obex-capability.xml", 19);
					strcpy(req2,"x-obex/capability");

					/* Type block */
					OBEXAddBlock(req, &Current, 0x42, req2, strlen(req2));
				} else if (s->Phone.Data.Priv.OBEXGEN.Service == OBEX_IRMC) {
					/* No file name? Grab devinfo in IrMC mode */
					smprintf(s, "No filename requested, grabbing device information as devinfo.txt\n");
					EncodeUnicode(File->Name, "devinfo.txt", 19);
					EncodeUnicode(req2,"telecom/devinfo.txt",19);

					/* Name block */
					OBEXAddBlock(req, &Current, 0x01, req2, UnicodeLength(req2)*2+2);
				} else {
					return ERR_NOTSUPPORTED;
				}
			} else {
				if (strcmp(s->CurrentConfig->Model,"seobex")) {
					smprintf(s,"Changing to root\n");
					error = OBEXGEN_ChangePath(s, NULL, 2);
					if (error != ERR_NONE) return error;

					Pos = 0;
					do {
						OBEXGEN_FindNextDir(File->ID_FullName, &Pos, req2);
						smprintf(s,"%s %i %zi\n",DecodeUnicodeString(req2),Pos,UnicodeLength(File->ID_FullName));
						if (Pos == UnicodeLength(File->ID_FullName)) break;
						smprintf(s,"Changing path down\n");
						error=OBEXGEN_ChangePath(s, req2, 2);
						if (error != ERR_NONE) return error;
					} while (1);
				} else {
					CopyUnicodeString(req2,File->ID_FullName);
				}
				CopyUnicodeString(File->Name,req2);

				s->Phone.Data.File = File;

				Current = 0;
				/* Name block */
				OBEXAddBlock(req, &Current, 0x01, req2, UnicodeLength(req2)*2+2);
			}
		}
	}

	s->Phone.Data.Priv.OBEXGEN.FileLastPart = false;

	if (s->Phone.Data.Priv.OBEXGEN.Service == OBEX_BrowsingFolders) {
		/* connection ID block */
		req[Current++] = 0xCB; // ID
		req[Current++] = 0x00; req[Current++] = 0x00;
		req[Current++] = 0x00; req[Current++] = 0x01;
	}

	smprintf(s, "Getting first file part from filesystem\n");
	error=GSM_WaitFor (s, req, Current, 0x83, 4, ID_GetFile);
	if (error != ERR_NONE) return error;

	while (!s->Phone.Data.Priv.OBEXGEN.FileLastPart) {
		Current = 0;
 	    	if (s->Phone.Data.Priv.OBEXGEN.Service == OBEX_BrowsingFolders) {
			/* connection ID block */
			req[Current++] = 0xCB; // ID
			req[Current++] = 0x00; req[Current++] = 0x00;
			req[Current++] = 0x00; req[Current++] = 0x01;
		}
		smprintf(s, "Getting file part from filesystem\n");
		error=GSM_WaitFor (s, req, Current, 0x83, 4, ID_GetFile);
		if (error != ERR_NONE) return error;
	}
	return ERR_EMPTY;
}

GSM_Error OBEXGEN_GetFilePart(GSM_StateMachine *s, GSM_File *File, int *Handle, int *Size)
{
	(*Size) = 0;
	return OBEXGEN_PrivGetFilePart(s,File,false);
}


/**
 * Merges filename from path and file
 */
void CreateFileName(unsigned char *Dest, unsigned char *Path, unsigned char *Name)
{
	size_t len;

	/* Folder name */
	CopyUnicodeString(Dest, Path);
	len = UnicodeLength(Dest);
	/* Append slash */
	if (len > 0) {
		Dest[2*len + 0] = 0;
		Dest[2*len + 1] = '/';
		len++;
	}
	/* And add filename */
	CopyUnicodeString(Dest + 2*len, Name);
}

/**
 * List OBEX folder.
 *
 * @todo: We assume XML reply is in UTF-8, but this doesn't have to be true.
 */
GSM_Error OBEXGEN_GetNextFileFolder(GSM_StateMachine *s, GSM_File *File, bool start)
{
	GSM_Phone_OBEXGENData	*Priv = &s->Phone.Data.Priv.OBEXGEN;
	GSM_Error		error;
	unsigned char		Line[500],Line2[500],*name,*size;
	int			Pos,i,j,num,pos2,Current,z;

	if (start) {
		Priv->Files[0].Folder		= true;
		Priv->Files[0].Level		= 1;
		Priv->Files[0].Name[0]		= 0;
		Priv->Files[0].Name[1]		= 0;
		Priv->Files[0].ID_FullName[0]	= 0;
		Priv->Files[0].ID_FullName[1]	= 0;

		Priv->FilesLocationsUsed 	= 1;
		Priv->FilesLocationsCurrent 	= 0;
		Priv->FileLev			= 0;

		smprintf(s,"Changing to root\n");
		error = OBEXGEN_ChangePath(s, NULL, 2);
		if (error != ERR_NONE) return error;

		Current = 0;
	}

	while (1) {
		if (Priv->FilesLocationsCurrent == Priv->FilesLocationsUsed) {
			dbgprintf("Last file\n");
			return ERR_EMPTY;
		}

		CopyUnicodeString(File->ID_FullName,Priv->Files[Priv->FilesLocationsCurrent].ID_FullName);
		File->Level	= Priv->Files[Priv->FilesLocationsCurrent].Level;
		File->Folder	= Priv->Files[Priv->FilesLocationsCurrent].Folder;
		CopyUnicodeString(File->Name,Priv->Files[Priv->FilesLocationsCurrent].Name);
		Priv->FilesLocationsCurrent++;

		if (File->Folder) {
			while (File->Level <= Priv->FileLev) {
				smprintf(s,"Changing path up (%d, %d)\n", File->Level, Priv->FileLev);
				/* Flag 1 means cd .. */
				error=OBEXGEN_ChangePath(s, NULL, 1 | 2);
				if (error != ERR_NONE) return error;
				Priv->FileLev--;
			}

			smprintf(s,"Level %i %i\n",File->Level,Priv->FileLev);

			File->Buffer		= NULL;
			File->Used		= 0;
			File->ModifiedEmpty	= true;

			OBEXGEN_PrivGetFilePart(s, File, true);

			num = 0;
			Pos = 0;
			/* Calculate number of files */
			while (1) {
				MyGetLine(File->Buffer, &Pos, Line, File->Used);
				if (strlen(Line) == 0) break;
				name = strstr(Line,"folder name=\"");
				if (name != NULL) {
					name += 13;
					j = 0;
					while(1) {
						if (name[j] == '"') break;
						j++;
					}
					name[j] = 0;

					if (strcmp(name,".")) num++;
				}
				name = strstr(Line,"file name=\"");
				if (name != NULL) num++;
			}
			/* Shift current files to the end of list */
			if (num != 0) {
				i = Priv->FilesLocationsUsed-1;
				while (1) {
					if (i==Priv->FilesLocationsCurrent-1) break;
					memcpy(&Priv->Files[i+num],&Priv->Files[i],sizeof(GSM_File));
					i--;
				}
			}

			/* Actually parse file listing */
			Pos 	= 0;
			pos2 	= 0;
			while (1) {
				MyGetLine(File->Buffer, &Pos, Line, File->Used);
				if (strlen(Line) == 0) break;
				strcpy(Line2,Line);
				name = strstr(Line2,"folder name=\"");
				if (name != NULL) {
					name += 13;
					j = 0;
					while(1) {
						if (name[j] == '"') break;
						j++;
					}
					name[j] = 0;
					if (strcmp(name,".")) {
						dbgprintf("copying folder %s to %i parent %i\n",name,Priv->FilesLocationsCurrent+pos2,Priv->FilesLocationsCurrent);
						/* Convert filename from UTF-8 */
						DecodeUTF8(Priv->Files[Priv->FilesLocationsCurrent+pos2].Name, name, strlen(name));
						/* Create file name from parts */
						CreateFileName(
							Priv->Files[Priv->FilesLocationsCurrent+pos2].ID_FullName,
							File->ID_FullName,
							Priv->Files[Priv->FilesLocationsCurrent+pos2].Name
							);
						Priv->Files[Priv->FilesLocationsCurrent+pos2].Level  = File->Level+1;
						Priv->Files[Priv->FilesLocationsCurrent+pos2].Folder = true;
						Priv->FilesLocationsUsed++;
						pos2++;
					}
				}
				strcpy(Line2,Line);
				name = strstr(Line2,"file name=\"");
				if (name != NULL) {
					name += 11;
					j = 0;
					while(1) {
						if (name[j] == '"') break;
						j++;
					}
					name[j] = 0;
					dbgprintf("copying file %s to %i\n",name,Priv->FilesLocationsCurrent+pos2);
					/* Convert filename from UTF-8 */
					DecodeUTF8(Priv->Files[Priv->FilesLocationsCurrent+pos2].Name, name, strlen(name));
					/* Create file name from parts */
					CreateFileName(
						Priv->Files[Priv->FilesLocationsCurrent+pos2].ID_FullName,
						File->ID_FullName,
						Priv->Files[Priv->FilesLocationsCurrent+pos2].Name
						);

					Priv->Files[Priv->FilesLocationsCurrent+pos2].Level	= File->Level+1;
					Priv->Files[Priv->FilesLocationsCurrent+pos2].Folder 	= false;
					Priv->Files[Priv->FilesLocationsCurrent+pos2].Used 	= 0;
					strcpy(Line2,Line);
					size = strstr(Line2,"size=\"");
					if (size != NULL) Priv->Files[Priv->FilesLocationsCurrent+pos2].Used = atoi(size+6);

					Priv->Files[Priv->FilesLocationsCurrent+pos2].ModifiedEmpty = true;
					strcpy(Line2,Line);
					size = strstr(Line2,"modified=\"");
					if (size != NULL) {
						Priv->Files[Priv->FilesLocationsCurrent+pos2].ModifiedEmpty = false;
						ReadVCALDateTime(size+10, &Priv->Files[Priv->FilesLocationsCurrent+pos2].Modified);
					}
					Priv->FilesLocationsUsed++;
					pos2++;
				}
			}

			z = Priv->FilesLocationsCurrent;
			if (z != 1) {
				while (1) {
					if (z == Priv->FilesLocationsUsed) break;
					if (Priv->Files[z].Folder) {
						if (Priv->Files[z].Level > File->Level) {
							smprintf(s,"Changing path down\n");
							error=OBEXGEN_ChangePath(s, File->Name, 2);
							if (error != ERR_NONE) return error;
							Priv->FileLev = File->Level;
						}
						break;
					}
					z++;
				}
			}

			free(File->Buffer);
		} else {
			File->Used 	    	= Priv->Files[Priv->FilesLocationsCurrent-1].Used;
			File->ModifiedEmpty 	= Priv->Files[Priv->FilesLocationsCurrent-1].ModifiedEmpty;
			if (!File->ModifiedEmpty) {
				memcpy(&File->Modified,&Priv->Files[Priv->FilesLocationsCurrent-1].Modified,sizeof(GSM_DateTime));
			}
			File->ReadOnly 		= false;
			File->Protected 	= false;
			File->Hidden		= false;
			File->System		= false;

		}
		return ERR_NONE;
	}
}

GSM_Error OBEXGEN_DeleteFile(GSM_StateMachine *s, unsigned char *ID)
{
	GSM_Error		error;
	unsigned int		Current = 0, Pos;
	unsigned char		req[200],req2[200];

	if (!strcmp(s->CurrentConfig->Model,"seobex")) return ERR_NOTSUPPORTED;

	smprintf(s,"Changing to root\n");
	error = OBEXGEN_ChangePath(s, NULL, 2);
	if (error != ERR_NONE) return error;

	Pos = 0;
	do {
		OBEXGEN_FindNextDir(ID, &Pos, req2);
		smprintf(s,"%s %i %zi\n",DecodeUnicodeString(req2),Pos,UnicodeLength(ID));
		if (Pos == UnicodeLength(ID)) break;
		smprintf(s,"Changing path down\n");
		error=OBEXGEN_ChangePath(s, req2, 2);
		if (error != ERR_NONE) return error;
	} while (1);

	/* Name block */
	OBEXAddBlock(req, &Current, 0x01, req2, UnicodeLength(req2)*2+2);

	/* connection ID block */
	req[Current++] = 0xCB; // ID
	req[Current++] = 0x00; req[Current++] = 0x00;
	req[Current++] = 0x00; req[Current++] = 0x01;

	return GSM_WaitFor (s, req, Current, 0x82, 4, ID_AddFile);
}

GSM_Error OBEXGEN_AddFolder(GSM_StateMachine *s, GSM_File *File)
{
	GSM_Error		error;
	unsigned char		req2[200];
	unsigned int		Pos;

	if (!strcmp(s->CurrentConfig->Model,"seobex")) return ERR_NOTSUPPORTED;

	smprintf(s,"Changing to root\n");
	error = OBEXGEN_ChangePath(s, NULL, 2);
	if (error != ERR_NONE) return error;

	Pos = 0;
	do {
		OBEXGEN_FindNextDir(File->ID_FullName, &Pos, req2);
		smprintf(s,"%s %i %zi\n",DecodeUnicodeString(req2),Pos,UnicodeLength(File->ID_FullName));
		smprintf(s,"Changing path down\n");
		error=OBEXGEN_ChangePath(s, req2, 2);
		if (error != ERR_NONE) return error;
		if (Pos == UnicodeLength(File->ID_FullName)) break;
	} while (1);

	smprintf(s,"Adding directory\n");
	return OBEXGEN_ChangePath(s, File->Name, 0);
}

GSM_Reply_Function OBEXGENReplyFunctions[] = {
	/* CONTINUE block */
	{OBEXGEN_ReplyAddFilePart,	"\x90",0x00,0x00,ID_AddFile			},
	{OBEXGEN_ReplyGetFilePart,	"\x90",0x00,0x00,ID_GetFile			},

	/* OK block */
	{OBEXGEN_ReplyChangePath,	"\xA0",0x00,0x00,ID_SetPath			},
	{OBEXGEN_ReplyConnect,		"\xA0",0x00,0x00,ID_Initialise			},
	{OBEXGEN_ReplyAddFilePart,	"\xA0",0x00,0x00,ID_AddFile			},
	{OBEXGEN_ReplyGetFilePart,	"\xA0",0x00,0x00,ID_GetFile			},

	/* FOLDER CREATED block */
	{OBEXGEN_ReplyChangePath,	"\xA1",0x00,0x00,ID_SetPath			},

	/* NOT UNDERSTAND block */
	{OBEXGEN_ReplyConnect,		"\xC0",0x00,0x00,ID_Initialise			},
	{OBEXGEN_ReplyGetFilePart,	"\xC0",0x00,0x00,ID_GetFile			},
	{OBEXGEN_ReplyAddFilePart,	"\xC0",0x00,0x00,ID_AddFile			},

	/* FORBIDDEN block */
	{OBEXGEN_ReplyChangePath,	"\xC3",0x00,0x00,ID_SetPath			},
	{OBEXGEN_ReplyGetFilePart,	"\xC3",0x00,0x00,ID_GetFile			},
	{OBEXGEN_ReplyAddFilePart,	"\xC3",0x00,0x00,ID_AddFile			},

	/* NOT FOUND block */
	{OBEXGEN_ReplyGetFilePart,	"\xC4",0x00,0x00,ID_GetFile			},
	{OBEXGEN_ReplyChangePath,	"\xC4",0x00,0x00,ID_SetPath			},

	{NULL,				"\x00",0x00,0x00,ID_None			}
};

GSM_Phone_Functions OBEXGENPhone = {
	"obex|seobex|obexirmc|obexnone",
	OBEXGENReplyFunctions,
	OBEXGEN_Initialise,
	NONEFUNCTION,			/*	Terminate 		*/
	GSM_DispatchMessage,
	NOTIMPLEMENTED,			/* 	ShowStartInfo		*/
	NONEFUNCTION,			/*	GetManufacturer		*/
	NONEFUNCTION,			/*	GetModel		*/
	NONEFUNCTION,			/*	GetFirmware		*/
	NOTIMPLEMENTED,			/*	GetIMEI			*/
	NOTIMPLEMENTED,			/*	GetOriginalIMEI		*/
	NOTIMPLEMENTED,			/*	GetManufactureMonth	*/
	NOTIMPLEMENTED,			/*	GetProductCode		*/
	NOTIMPLEMENTED,			/*	GetHardware		*/
	NOTIMPLEMENTED,			/*	GetPPM			*/
	NOTIMPLEMENTED,			/*	GetSIMIMSI		*/
	NOTIMPLEMENTED,			/*	GetDateTime		*/
	NOTIMPLEMENTED,			/*	SetDateTime		*/
	NOTIMPLEMENTED,			/*	GetAlarm		*/
	NOTIMPLEMENTED,			/*	SetAlarm		*/
	NOTSUPPORTED,			/* 	GetLocale		*/
	NOTSUPPORTED,			/* 	SetLocale		*/
	NOTIMPLEMENTED,			/*	PressKey		*/
	NOTIMPLEMENTED,			/*	Reset			*/
	NOTIMPLEMENTED,			/*	ResetPhoneSettings	*/
	NOTIMPLEMENTED,			/*	EnterSecurityCode	*/
	NOTIMPLEMENTED,			/*	GetSecurityStatus	*/
	NOTIMPLEMENTED,			/*	GetDisplayStatus	*/
	NOTIMPLEMENTED,			/*	SetAutoNetworkLogin	*/
	NOTIMPLEMENTED,			/*	GetBatteryCharge	*/
	NOTIMPLEMENTED,			/*	GetSignalQuality	*/
	NOTIMPLEMENTED,			/*	GetNetworkInfo		*/
	NOTIMPLEMENTED,     		/*  	GetCategory 		*/
 	NOTSUPPORTED,       		/*  	AddCategory 		*/
        NOTIMPLEMENTED,      		/*  	GetCategoryStatus 	*/
	NOTIMPLEMENTED,			/*	GetMemoryStatus		*/
	NOTIMPLEMENTED,			/*	GetMemory		*/
	NOTIMPLEMENTED,			/*	GetNextMemory		*/
	NOTIMPLEMENTED,			/*	SetMemory		*/
	NOTIMPLEMENTED,			/*	AddMemory		*/
	NOTIMPLEMENTED,			/*	DeleteMemory		*/
	NOTIMPLEMENTED,			/*	DeleteAllMemory		*/
	NOTIMPLEMENTED,			/*	GetSpeedDial		*/
	NOTIMPLEMENTED,			/*	SetSpeedDial		*/
	NOTIMPLEMENTED,			/*	GetSMSC			*/
	NOTIMPLEMENTED,			/*	SetSMSC			*/
	NOTIMPLEMENTED,			/*	GetSMSStatus		*/
	NOTIMPLEMENTED,			/*	GetSMS			*/
	NOTIMPLEMENTED,			/*	GetNextSMS		*/
	NOTIMPLEMENTED,			/*	SetSMS			*/
	NOTIMPLEMENTED,			/*	AddSMS			*/
	NOTIMPLEMENTED,			/* 	DeleteSMS 		*/
	NOTIMPLEMENTED,			/*	SendSMSMessage		*/
	NOTSUPPORTED,			/*	SendSavedSMS		*/
	NOTSUPPORTED,			/*	SetFastSMSSending	*/
	NOTIMPLEMENTED,			/*	SetIncomingSMS		*/
	NOTIMPLEMENTED,			/* 	SetIncomingCB		*/
	NOTIMPLEMENTED,			/*	GetSMSFolders		*/
 	NOTIMPLEMENTED,			/* 	AddSMSFolder		*/
 	NOTIMPLEMENTED,			/* 	DeleteSMSFolder		*/
	NOTIMPLEMENTED,			/*	DialVoice		*/
	NOTIMPLEMENTED,			/*	AnswerCall		*/
	NOTIMPLEMENTED,			/*	CancelCall		*/
 	NOTIMPLEMENTED,			/* 	HoldCall 		*/
 	NOTIMPLEMENTED,			/* 	UnholdCall 		*/
 	NOTIMPLEMENTED,			/* 	ConferenceCall 		*/
 	NOTIMPLEMENTED,			/* 	SplitCall		*/
 	NOTIMPLEMENTED,			/* 	TransferCall		*/
 	NOTIMPLEMENTED,			/* 	SwitchCall		*/
 	NOTIMPLEMENTED,			/* 	GetCallDivert		*/
 	NOTIMPLEMENTED,			/* 	SetCallDivert		*/
 	NOTIMPLEMENTED,			/* 	CancelAllDiverts	*/
	NOTIMPLEMENTED,			/*	SetIncomingCall		*/
	NOTIMPLEMENTED,			/*  	SetIncomingUSSD		*/
	NOTIMPLEMENTED,			/*	SendDTMF		*/
	NOTIMPLEMENTED,			/*	GetRingtone		*/
	NOTIMPLEMENTED,			/*	SetRingtone		*/
	NOTIMPLEMENTED,			/*	GetRingtonesInfo	*/
	NOTIMPLEMENTED,			/* 	DeleteUserRingtones	*/
	NOTIMPLEMENTED,			/*	PlayTone		*/
	NOTIMPLEMENTED,			/*	GetWAPBookmark		*/
	NOTIMPLEMENTED,			/* 	SetWAPBookmark 		*/
	NOTIMPLEMENTED, 		/* 	DeleteWAPBookmark 	*/
	NOTIMPLEMENTED,			/* 	GetWAPSettings 		*/
	NOTIMPLEMENTED,			/* 	SetWAPSettings 		*/
	NOTSUPPORTED,			/*	GetSyncMLSettings	*/
	NOTSUPPORTED,			/*	SetSyncMLSettings	*/
	NOTSUPPORTED,			/*	GetChatSettings		*/
	NOTSUPPORTED,			/*	SetChatSettings		*/
	NOTSUPPORTED,			/* 	GetMMSSettings		*/
	NOTSUPPORTED,			/* 	SetMMSSettings		*/
	NOTSUPPORTED,			/*	GetMMSFolders		*/
	NOTSUPPORTED,			/*	GetNextMMSFileInfo	*/
	NOTIMPLEMENTED,			/*	GetBitmap		*/
	NOTIMPLEMENTED,			/*	SetBitmap		*/
	NOTIMPLEMENTED,			/*	GetToDoStatus		*/
	NOTIMPLEMENTED,			/*	GetToDo			*/
	NOTIMPLEMENTED,			/*	GetNextToDo		*/
	NOTIMPLEMENTED,			/*	SetToDo			*/
	NOTIMPLEMENTED,			/*	AddToDo			*/
	NOTIMPLEMENTED,			/*	DeleteToDo		*/
	NOTIMPLEMENTED,			/*	DeleteAllToDo		*/
	NOTIMPLEMENTED,			/*	GetCalendarStatus	*/
	NOTIMPLEMENTED,			/*	GetCalendar		*/
    	NOTIMPLEMENTED,			/*  	GetNextCalendar		*/
	NOTIMPLEMENTED,			/*	SetCalendar		*/
	NOTIMPLEMENTED,			/*	AddCalendar		*/
	NOTIMPLEMENTED,			/*	DeleteCalendar		*/
	NOTIMPLEMENTED,			/*	DeleteAllCalendar	*/
	NOTSUPPORTED,			/* 	GetCalendarSettings	*/
	NOTSUPPORTED,			/* 	SetCalendarSettings	*/
	NOTSUPPORTED,			/*	GetNoteStatus		*/
	NOTSUPPORTED,			/*	GetNote			*/
	NOTSUPPORTED,			/*	GetNextNote		*/
	NOTSUPPORTED,			/*	SetNote			*/
	NOTSUPPORTED,			/*	AddNote			*/
	NOTSUPPORTED,			/* 	DeleteNote		*/
	NOTSUPPORTED,			/*	DeleteAllNotes		*/
	NOTIMPLEMENTED, 		/*	GetProfile		*/
	NOTIMPLEMENTED, 		/*	SetProfile		*/
    	NOTIMPLEMENTED,			/*  	GetFMStation        	*/
    	NOTIMPLEMENTED,			/*  	SetFMStation        	*/
    	NOTIMPLEMENTED,			/*  	ClearFMStations       	*/
	OBEXGEN_GetNextFileFolder,
	NOTSUPPORTED,			/*	GetFolderListing	*/
	NOTSUPPORTED,			/*	GetNextRootFolder	*/
	NOTSUPPORTED,			/*	SetFileAttributes	*/
	OBEXGEN_GetFilePart,
	OBEXGEN_AddFilePart,
	NOTIMPLEMENTED, 		/* 	GetFileSystemStatus	*/
	OBEXGEN_DeleteFile,
	OBEXGEN_AddFolder,
	NOTSUPPORTED,			/* 	DeleteFolder		*/
	NOTIMPLEMENTED,			/* 	GetGPRSAccessPoint	*/
	NOTIMPLEMENTED			/* 	SetGPRSAccessPoint	*/
};

#endif

/* How should editor hadle tabs in this file? Add editor commands here.
 * vim: noexpandtab sw=8 ts=8 sts=8:
 */
