#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include "Lyric_storage.h"
#include "ID3_parse.h"

#ifdef USEMEMWATCH
#include "memwatch.h"
#endif

int SearchPhrase(FILE *inf,unsigned long breakpos,unsigned long *pos,const unsigned char *phrase,unsigned long phraselen,unsigned char autoseek)
{
	unsigned long originalpos=0;
	unsigned long matchpos=0;
	unsigned char c=0;
	unsigned long ctr=0;		//Used to index into the phrase[] array, beginning with the first character
	unsigned char success=0;
	unsigned long currentpos=0;	//Store the current file position

//Validate input
	if(!inf || !phrase)	//These input parameters are not allowed to be NULL
		return -1;

	if(!phraselen)
		return 0;	//There will be no matches to an empty search array

//Initialize for search
	errno=0;
	originalpos=ftell(inf);	//Store the original file position
	currentpos=originalpos;
	if(errno)		//If there was an I/O error
		return -1;

	c=fgetc(inf);		//Read the first character of the file
	currentpos++;		//Track that one more byte has been read
	if(ferror(inf))		//If there was an I/O error
	{
		fseek(inf,originalpos,SEEK_SET);
		return -1;
	}

//Perform search
	while(!feof(inf))	//While end of file hasn't been reached
	{
		if(breakpos != 0)
		{
/*v2.3	Keep track of file position instead of querying every iteration
			currentpos=ftell(inf);	//Check to see if the exit position has been reached
			if(ferror(inf))				//If there was an I/O error
			{
				fseek(inf,originalpos,SEEK_SET);
				return -1;
			}
*/
			if(currentpos >= breakpos)	//If the exit position was reached
			{
				fseek(inf,originalpos,SEEK_SET);
				return 0;				//Return no match
			}
		}

	//Check if the next character in the search phrase has been matched
		if(c == phrase[ctr])
		{	//The next character was matched
			if(ctr == 0)	//The match was with the first character in the search array
			{
/*v2.3	Keep track of file position instead of querying every iteration
				matchpos=ftell(inf)-1;	//Store the position of this potential match (rewound one byte)
				if(ferror(inf))			//If there was an I/O error
				{
					fseek(inf,originalpos,SEEK_SET);
					return -1;
				}
*/
				matchpos=currentpos-1;	//Store the position of this potential match (rewound one byte)
			}
			ctr++;	//Advance to the next character in search array
			if(ctr == phraselen)
			{	//If all characters have been matched
				success=1;
				break;
			}
		}
		else	//Character did not match
			ctr=0;			//Ensure that the first character in the search array is looked for

		c=fgetc(inf);	//Read the next character of the file
		currentpos++;	//Track that one more byte has been read
	}

//Seek to the appropriate file position
	if(success && autoseek)		//If we should seek to the successful match
		fseek(inf,matchpos,SEEK_SET);
	else				//If we should return to the original file position
		fseek(inf,originalpos,SEEK_SET);

	if(ferror(inf))		//If there was an I/O error
		return -1;

//Return match/non match
	if(success)
	{
		if(pos != NULL)
			*pos=matchpos;
		return 1;	//Return match
	}

	return 0;	//Return no match
}

char *ReadTextInfoFrame(FILE *inf)
{
	//Expects the file position of inf to be at the beginning of an ID3 Frame header for a Text Information Frame
	//The first byte read (which will be from the Frame ID) is expected to be 'T' as per ID3v2 specifications
	//Reads and returns the string from the frame in a newly-allocated string
	//NULL is returned if the encoding is Unicode, if the frame is malformed or if there is an I/O error
	unsigned long originalpos=0;
	char buffer[11]={0};	//Used to store the ID3 frame header and the string encoding byte
	char *string=NULL;
	unsigned long framesize=0;

	if(inf == NULL)
		return NULL;

//Store the original file position
	originalpos=ftell(inf);
	if(ferror(inf))		//If there was a file I/O error
		return NULL;

	if(fread(buffer,11,1,inf) != 1)	//Read the frame header and the following byte (string encoding)
		return NULL;			//If there was an I/O error, return error

//Validate frame ID begins with 'T', ensure that the compression and encryption bits are not set, ensure Unicode string encoding is not specified
	if((buffer[0] != 'T') || ((buffer[9] & 192) != 0) || (buffer[10] != 0))
	{
		fseek(inf,originalpos,SEEK_SET);	//Return to original file position
		return NULL;				//Return error
	}

//Read the frame size as a Big Endian integer (4 bytes)
	framesize=((unsigned long)buffer[4]<<24) | ((unsigned long)buffer[5]<<16) | ((unsigned long)buffer[6]<<8) | ((unsigned long)buffer[7]);	//Convert to 4 byte integer

	if(framesize < 2)	//The smallest valid frame size is 2, one for the encoding type and one for a null terminator (empty string)
	{
		fseek(inf,originalpos,SEEK_SET);	//Return to original file position
		return NULL;
	}

	string=malloc(framesize);	//Allocate enough space to store the data (one less than the framesize, yet one more for NULL terminator)
	if(!string)			//Memory allocation failure
	{
		fseek(inf,originalpos,SEEK_SET);	//Return to original file position
		return NULL;
	}

//Read the data and NULL terminate the string
	if(fread(string,framesize-1,1,inf) != 1)	//If the data failed to be read
	{
		fseek(inf,originalpos,SEEK_SET);	//Return to original file position
		free(string);
		return NULL;
	}
	string[framesize-1]='\0';

	return string;
}

/*v2.3	Rewrote to list all frames that are present
int DisplayID3Tag(char *filename)
{
//	const char *tags[]={"TPE1","TIT2","TYER"};	//The three tags to try to find in the file
//V2.3	Added the entire list of all Text Information frames defined by the ID3v2 standard
	const char *tags[]={"TALB","TBPM","TCOM","TCON","TCOP","TDAT","TDLY","TENC","TEXT","TFLT","TIME","TIT1","TIT2","TIT3","TKEY","TLAN","TLEN","TMED","TOAL","TOFN","TOLY","TOPE","TORY","TOWN","TPE1","TPE2","TPE3","TPE4","TPOS","TPUB","TRCK","TRDA","TRSN","TRSO","TSIZ","TSRC","TSSE","TYER"};
	char buffer[1024]={0};	//A buffer to load strings into
//	char *temp;
	unsigned long ctr;
//	unsigned long ctr2,length;	//Used to validate year tag
//	unsigned char yearvalid=1;	//Used to validate year tag
	unsigned char tagcount=0;
	struct ID3Tag tag={NULL,0,0,0,0,0,0.0,NULL};

//Validate parameters
	if(!filename)
		return 0;

	tag.fp=fopen(filename,"rb");
	if(tag.fp == NULL)
		return 0;

	if(FindID3Tag(&tag) == 0)	//Find start and end of ID3 tag
	{
		fclose(tag.fp);
		return 0;
	}

	if(ID3FrameProcessor(&tag) == 0)	//Build a list of the ID3 frames
	{
		fclose(tag.fp);
		return 0;
	}

	for(ctr=0;ctr<sizeof(tags)/sizeof(const char *);ctr++)
	{	//For each tag we're looking for
**v2.3	Redesigned code to search for frame IDs
		fseek(tag.fp,tag.framestart,SEEK_SET);	//Seek to first frame
		if(ferror(tag.fp))		//If there was a file I/O error
		{
			fclose(tag.fp);
			return 0;
		}

		if(SearchPhrase(tag.fp,tag.tagend,NULL,tags[ctr],4,1) == 1)
		{	//If there is a match for this tag, seek to it
			temp=ReadTextInfoFrame(tag.fp);	//Attempt to read it

			if(temp != NULL)
			{
				switch(ctr)
				{
					case 0: //Store Performer tag, truncated to fit
						if(strlen(temp) > 255)	//If this string won't fit without overflowing
							temp[255] = '\0';	//Make it fit

						#ifdef EOF_BUILD
						strcpy(artist,temp);
						#else
						printf("Tag: %s = \"%s\"\n",tags[ctr],temp);
						#endif

						free(temp);
					break;

					case 1:	//Store Title tag, truncated to fit
						if(strlen(temp) > 255)	//If this string won't fit without overflowing
							temp[255] = '\0';	//Make it fit

						#ifdef EOF_BUILD
						strcpy(title,temp);
						#else
						printf("Tag: %s = \"%s\"\n",tags[ctr],temp);
						#endif

						free(temp);
					break;

					case 2:	//Store Year tag, after it's validated
						length=strlen(temp);
						if(length < 5)
						{	//If the string is no more than 4 characters
							for(ctr2=0;ctr2<length;ctr2++)	//Check all digits to ensure they're numerical
								if(!isdigit(temp[ctr2]))
									yearvalid=0;

							if(yearvalid)
							{
								#ifdef EOF_BUILD
								strcpy(year,temp);
								#else
								printf("Tag: %s = \"%s\"\n",tags[ctr],temp);
								#endif
							}

							free(temp);
						}
					break;

					default:	//This shouldn't be reachable
						free(temp);
						fclose(tag.fp);
					return tagcount;
				}

				tagcount++;	//One more tag was read
			}//if(temp != NULL)
		}//If there is a match for this tag
**
		if(GrabID3TextFrame(&tag,tags[ctr],buffer,sizeof(buffer)/sizeof(char)) != NULL)
		{	//If the specified text info frame was found and loaded into the buffer
			printf("Frame: %s = \"%s\"\n",tags[ctr],buffer);
		}
	}//For each tag we're looking for

	if(FindID3Frame(&tag,"SYLT") != NULL)
		puts("SYLT frame present");

	DestroyID3FrameList(&tag);	//Release the list of ID3 frames
	fclose(tag.fp);
	return tagcount;
}
*/

int DisplayID3Tag(char *filename)
{
	const char *tags[]={"TALB","TBPM","TCOM","TCON","TCOP","TDAT","TDLY","TENC","TEXT","TFLT","TIME","TIT1","TIT2","TIT3","TKEY","TLAN","TLEN","TMED","TOAL","TOFN","TOLY","TOPE","TORY","TOWN","TPE1","TPE2","TPE3","TPE4","TPOS","TPUB","TRCK","TRDA","TRSN","TRSO","TSIZ","TSRC","TSSE","TYER"};
	char *buffer=NULL;	//A buffer to store strings returned from ReadTextInfoFrame()
	unsigned long ctr;
	unsigned char tagcount=0;
	struct ID3Tag tag={NULL,0,0,0,0,0,0.0,NULL};
	struct ID3Frame *temp=NULL;	//Conductor for the ID3 frames list
	char found=0;			//Used to track whether each frame was a text info frame defined in the tags[] array above

//Validate parameters
	if(!filename)
		return 0;

	tag.fp=fopen(filename,"rb");
	if(tag.fp == NULL)
		return 0;

	if(FindID3Tag(&tag) == 0)	//Find start and end of ID3 tag
	{
		fclose(tag.fp);
		return 0;
	}

	if(ID3FrameProcessor(&tag) == 0)	//Build a list of the ID3 frames
	{
		fclose(tag.fp);
		return 0;
	}

	for(temp=tag.frames;temp != NULL;temp=temp->next)
	{	//For each frame that was parsed, check the tags[] array to see if it is a text info frame
		found=0;	//Reset found status
		assert_wrapper(temp->frameid != NULL);
		for(ctr=0;ctr<sizeof(tags)/sizeof(const char *);ctr++)
		{	//For each defined text info frame id
			if(strcasecmp(tags[ctr],temp->frameid) == 0)
			{	//If the specified text info frame id matches this ID3 frame
				fseek_err(tag.fp,temp->pos,SEEK_SET);	//Seek to the ID3 frame in question
				buffer=ReadTextInfoFrame(tag.fp);	//Read and store the text info frame content
				if(buffer == NULL)
					printf("Frame: %s = Unreadable or Unicode format\n",tags[ctr]);
				else
					printf("Frame: %s = \"%s\"\n",tags[ctr],buffer);
				free(buffer);	//Release text info frame content
				buffer=NULL;
				found=1;
				break;	//Stop searching the tags[] array
			}
		}//For each defined text info frame id

		if(!found)
			printf("%s frame present\n",temp->frameid);
	}//For each frame that was parsed


	DestroyID3FrameList(&tag);	//Release the list of ID3 frames
	fclose(tag.fp);
	return tagcount;
}

int FindID3Tag(struct ID3Tag *ptr)
{
	unsigned long tagpos;
	unsigned char tagid[4]={'I','D','3',3};	//An ID3 tag will have this in the header
	unsigned char header[10]={0};	//Used to store the ID3 tag header
	unsigned char exheader[10]={0};	//The extended header is 6 bytes (plus 4 more if CRC is present)
	unsigned long tagsize;

	if((ptr == NULL) || (ptr->fp == NULL))
		return 0;	//Return failure

	if(SearchPhrase(ptr->fp,ptr->tagend,&tagpos,tagid,4,1) == 1)	//Search for and seek to ID3 tag header
	{
//		fseek(ptr->fp,tagpos,SEEK_SET);		//Seek to the ID3 tag header
		if(ferror(ptr->fp))
			return 0;	//Return failure upon file I/O error

		if(fread(header,10,1,ptr->fp) != 1)	//Read ID3 header
			return 0;	//Return failure upon I/O error

//Examine the flag in the header pertaining to extended header.  Parse the extended header if applicable
		if(header[5] & 64)	//If the extended header present bit is set
			if(fread(exheader,6,1,ptr->fp) != 1)	//Read extended header
				return 0;	//Return failure upon I/O error

//If applicable, examine the CRC present flag in the extended header.  Parse the CRC data if applicable
		if(exheader[4] & 128)	//If the CRC present bit is set
			if(fread(&(exheader[6]),4,1,ptr->fp) != 1)	//Read CRC data
				return 0;	//Return failure upon I/O error

		ptr->framestart=ftell(ptr->fp);	//Store the file position of the first byte beyond the header (first ID3 frame)

	//Calculate the tag size.  The MSB of each byte is ignored, but otherwise the 4 bytes are treated as Big Endian
		tagsize=(((header[6] & 127) << 21) | ((header[7] & 127) << 14) | ((header[8] & 127) << 7) | (header[9] & 127));

	//Calculate the position of the first byte outside the ID3 tag: tagpos + tagsize + tag header size
		ptr->tagstart=tagpos;
		ptr->tagend=ptr->framestart+tagsize;

		if(Lyrics.verbose>=2)
			printf("ID3 tag info:\n\tBegins at byte 0x%lX\n\tEnds after byte 0x%lX\n\tTag size is %ld bytes\n\tFirst frame begins at byte 0x%lX\n\n",ptr->tagstart,ptr->tagend,tagsize,ptr->framestart);

		return 1;
	}

	return 0;
}

unsigned long GetMP3FrameDuration(struct ID3Tag *ptr)
{
	unsigned char buffer[4];
	unsigned long samplerate=0;
	unsigned long samplesperframe=0;

	if((ptr == NULL) || (ptr->fp == NULL))
		return 0;

//Read the 4 byte frame header at the current file position
	errno=0;
	if(fread(buffer,4,1,ptr->fp) != 1)	//If there was a file I/O error
		return 0;

//Check for the "Frame Sync", where the first 11 bits of the frame header are set
	if((buffer[0] != 255) || ((buffer[1] & 224) != 224))	//If the first 11 bits are not set
		return 0;					//Return error

//Check the MPEG audio version ID, located immediately after the frame sync (bits 4 and 5 of the second byte)
//Then mask the fifth and sixth bit of the third header byte to determine the sample rate
	switch(buffer[1] & 24)	//Mask out all bits except 4 and 5
	{
		case 0:	//0,0	Nonstandard MPEG version 2.5
			if(Lyrics.verbose >= 2)
				puts("MP3 info:\n\tNonstandard MPEG version 2.5");
			switch(buffer[2] & 12)	//Mask out all bits except 5 and 6
			{
				case 0:	//0,0
					if(Lyrics.verbose >= 2)
						puts("\tSample rate: 11025 Hz");
					samplerate=11025;
				break;

				case 4:	//0,1
					if(Lyrics.verbose >= 2)
						puts("\tSample rate: 12000 Hz");
					samplerate=12000;
				break;

				case 8: //1,0
					if(Lyrics.verbose >= 2)
						puts("\tSample rate: 8000 Hz");
					samplerate=8000;
				break;

				case 12://1,1	Reserved
				default:
				return 0;
			}
		break;

		case 8: //0,1	Reserved
		return 0;

		case 16://1,0	MPEG version 2
			if(Lyrics.verbose >= 2)
				puts("MP3 info:\n\tMPEG version 2");
			switch(buffer[2] & 12)	//Mask out all bits except 5 and 6
			{
				case 0:	//0,0
					if(Lyrics.verbose >= 2)
						puts("\tSample rate: 22050 Hz");
					samplerate=22050;
				break;

				case 4:	//0,1
					if(Lyrics.verbose >= 2)
						puts("\tSample rate: 24000 Hz");
					samplerate=24000;
				break;

				case 8: //1,0
					if(Lyrics.verbose >= 2)
						puts("\tSample rate: 16000 Hz");
					samplerate=16000;
				break;

				case 12://1,1	Reserved
				default:
				return 0;
			}
		break;

		case 24://1,1	MPEG version 1
			if(Lyrics.verbose >= 2)
				puts("MP3 info:\n\tMPEG version 1");
			switch(buffer[2] & 12)	//Mask out all bits except 5 and 6
			{
				case 0:	//0,0
					if(Lyrics.verbose >= 2)
						puts("\tSample rate: 44100 Hz");
					samplerate=44100;
				break;

				case 4:	//0,1
					if(Lyrics.verbose >= 2)
						puts("\tSample rate: 48000 Hz");
					samplerate=48000;
				break;

				case 8: //1,0
					if(Lyrics.verbose >= 2)
						puts("\tSample rate: 32000 Hz");
					samplerate=32000;
				break;

				case 12://1,1	Reserved
				default:
				return 0;
			}
		break;

		default:	//Not possible
		return 0;
	}

//Check the Layer Description, located immediately after the MPEG audio version ID (bits 6 and 7 of the second header byte)
	switch(buffer[1] & 6)	//Mask out all bits except 6 and 7
	{
		case 0:	//0,0	Reserved
		return 0;

		case 2:	//0,1	Layer 3
			if(Lyrics.verbose >= 2)
				puts("\tLayer 3");
			samplesperframe=1152;
		break;

		case 4: //1,0	Layer 2
			if(Lyrics.verbose >= 2)
				puts("\tLayer 2");
			samplesperframe=1152;
		break;

		case 6: //1,1	Layer 1
			if(Lyrics.verbose >= 2)
				puts("\tLayer 1");
			samplesperframe=384;
		break;

		default:
		return 0;
	}

	ptr->samplerate=samplerate;
	ptr->samplesperframe=samplesperframe;
	ptr->frameduration=(double)samplesperframe * 1000.0 / (double)samplerate;

	if(Lyrics.verbose >= 2)
		printf("\tDuration of one MPEG frame is %fms\n\n",ptr->frameduration);

	return samplerate;
}

void ID3_Load(FILE *inf)
{	//Parses the file looking for an ID3 tag.  If found, the first MP3 frame is examined to obtain the sample rate
	//Then an SYLT frame is searched for within the ID3 tag.  If found, synchronized lyrics are imported
	struct ID3Tag tag={NULL,0,0,0,0,0,0.0,NULL};
	struct ID3Frame *frameptr=NULL;
	struct ID3Frame *frameptr2=NULL;

	assert_wrapper(inf != NULL);	//This must not be NULL
	tag.fp=inf;

	if(Lyrics.verbose)	printf("Importing ID3 lyrics from file \"%s\"\n\nParsing input MPEG audio file\n",Lyrics.infilename);

//Parse ID3 tag header
	if(FindID3Tag(&tag) == 0)	//Find start and end of ID3 tag
		return;			//Return if no ID3 tag was found
	if(ID3FrameProcessor(&tag) == 0)	//Build a list of the ID3 frames
	{
		fclose(tag.fp);
		return;	//Return if no frames were successfully parsed
	}

//Parse MPEG frame header
	fseek_err(tag.fp,tag.tagend,SEEK_SET);	//Seek to the first MP3 frame (immediately after the ID3 tag)
	if(GetMP3FrameDuration(&tag) == 0)	//Find the sample rate defined in the MP3 frame
		return;			//Return if the sample rate was not found or was invalid

//v2.3	Updated ID3 import to use the frame linked list
//	fseek_err(tag.fp,tag.framestart,SEEK_SET);	//Seek to first ID3 frame
//	if(SearchPhrase(tag.fp,tag.tagend,NULL,"SYLT",4,1) == 1)	//Seek to SYLT ID3 frame
	frameptr=FindID3Frame(&tag,"SYLT");		//Search ID3 frame list for SYLT ID3 frame
	frameptr2=FindID3Frame(&tag,"USLT");	//Search ID3 frame list for USLT ID3 frame
	if(frameptr != NULL)
	{	//Perform Synchronized ID3 Lyric import
		fseek_err(tag.fp,frameptr->pos,SEEK_SET);	//Seek to SYLT ID3 frame
		SYLT_Parse(&tag);
	}
//	else if(SearchPhrase(tag.fp,tag.tagend,NULL,"USLT",4,1) == 1)	//Seek to USLT ID3 frame
	else if(frameptr2 != NULL)
	{	//Perform Unsynchronized ID3 Lyric import
		puts("\aUnsynchronized ID3 lyric import currently not supported");
		exit_wrapper(1);
	}
	else
		return;							//Return if neither lyric frame is present

//Load song tags
	Lyrics.Title=GrabID3TextFrame(&tag,"TIT2",NULL,0);	//Return the defined song title, if it exists
	Lyrics.Artist=GrabID3TextFrame(&tag,"TPE1",NULL,0);	//Return the defined artist, if it exists
	Lyrics.Album=GrabID3TextFrame(&tag,"TALB",NULL,0);	//Return the defined album, if it exists

	ForceEndLyricLine();
	DestroyID3FrameList(&tag);	//Release the list of ID3 frames

	if(Lyrics.verbose)	printf("ID3 import complete.  %lu lyrics loaded\n\n",Lyrics.piececount);
}

void SYLT_Parse(struct ID3Tag *tag)
{
	unsigned char frameheader[10]={0};	//This is the ID3 frame header
	unsigned char syltheader[6]={0};	//This is the SYLT frame header, excluding the terminated descriptor string that follows
	char *contentdescriptor=NULL;		//The null terminated string located after the SYLT frame header
	char timestampformat=0;				//Will be set based on the SYLT header (1= MPEG Frames, 2=Milliseconds)
	char *string=NULL;					//Used to store SYLT strings that are read
	char *string2=NULL;					//Used to build a display version of the string for debug output (minus newline)
	unsigned char timestamparray[4]={0};//Used to store the timestamp for a lyric string
	unsigned long timestamp=0;			//The timestamp converted to milliseconds
	unsigned long breakpos=0;			//Will be set to the first byte beyond the SYLT frame
	unsigned long framesize=0;
	char groupswithnext=0;				//Used for grouping logic
	char linebreaks=0;					//Tracks whether the imported lyrics defined linebreaks (if not, each ID3 lyric should be treated as one line)
	struct Lyric_Piece *ptr=NULL,*ptr2=NULL;		//Used to insert line breaks as necessary
	struct Lyric_Line *lineptr=NULL;				//Used to insert line breaks as necessary
	unsigned long length=0;				//Used to store the length of each string that is read from file
	unsigned long filepos=0;			//Used to track the current file position during SYLT lyric parsing

//Validate input
	assert_wrapper((tag != NULL) && (tag->fp != NULL));
	breakpos=ftell_err(tag->fp);

//Load ID3 frame header
	fread_err(frameheader,10,1,tag->fp);	//Load ID3 frame header
	fread_err(syltheader,6,1,tag->fp);		//Load SYLT frame header
	contentdescriptor=ReadString(tag->fp,NULL);	//Load content descriptor string

	if(contentdescriptor == NULL)	//If the content descriptor string couldn't be read
	{
		puts("Damaged Content Descriptor String\nAborting");
		exit_wrapper(1);
	}

//This program doesn't currently look for more than the first SYLT frame

//Process headers
	if((frameheader[9] & 192) != 0)
	{
		puts("ID3 Compression and Encryption are not supported\nAborting");
		exit_wrapper(3);
	}

	if(syltheader[0] != 0)
	{
		puts("Unicode ID3 lyrics are not supported\nAborting");
		exit_wrapper(4);
	}

	timestampformat=syltheader[4];
	if((timestampformat != 1) && (timestampformat != 2))
	{
		printf("Warning:  Invalid timestamp format (%d) specified, ms timing assumed\n",timestampformat);
		timestampformat=2;	//Assume millisecond timing
	}

	//Process framesize as a 4 byte Big Endian integer
	framesize=((unsigned long)frameheader[4]<<24) | ((unsigned long)frameheader[5]<<16) | ((unsigned long)frameheader[6]<<8) | ((unsigned long)frameheader[7]);	//Convert to 4 byte integer
	breakpos=breakpos + framesize + 10;	//Find the position that is one byte past the end of the SYLT frame

	if(Lyrics.verbose>=2)
		printf("SYLT frame info:\n\tFrame size is %lu bytes\n\tEnds after byte 0x%lX\n\tTimestamp format: %s\n\tLanguage: %c%c%c\n\tContent Type %d\n\tContent Descriptor: \"%s\"\n\n",framesize,breakpos,timestampformat == 1 ? "MPEG frames" : "Milliseconds",syltheader[1],syltheader[2],syltheader[3],syltheader[5],contentdescriptor != NULL ? contentdescriptor : "(none)");

	if(Lyrics.verbose)
		puts("Parsing SYLT frame:");

	free(contentdescriptor);	//Release this, it's not going to be used
	contentdescriptor=NULL;

//Load SYLT lyrics
	filepos=ftell_err(tag->fp);
//v2.3	Optimized this loop to track the file position manually instead of querying for it each iteration
//	while((unsigned long)ftell_err(tag->fp) < breakpos)	//While we haven't reached the end of the SYLT frame
	while(filepos < breakpos)	//While we haven't reached the end of the SYLT frame
	{
	//Load the lyric text
		string=ReadString(tag->fp,&length);	//Load SYLT lyric string, save the string length
		if(string == NULL)
		{
			puts("Invalid SYLT lyric string\nAborting");
			exit_wrapper(6);
		}

	//Load the timestamp
		if(fread(timestamparray,4,1,tag->fp) != 1)	//Read timestamp
		{
			puts("Error reading SYLT timestamp\nAborting");
			exit_wrapper(7);
		}

		filepos+=length+4;	//The number of bytes read from the input file during this iteration is the string length and the timestamp length

	//Process the timestamp as a 4 byte Big Endian integer
		timestamp=(unsigned long)((timestamparray[0]<<24) | (timestamparray[1]<<16) | (timestamparray[2]<<8) | timestamparray[3]);

		if(timestampformat == 1)	//If this timestamp is in MPEG frames instead of milliseconds
			timestamp=((double)timestamp * tag->frameduration + 0.5);	//Convert to milliseconds, rounding up

	//Perform line break logic
		if((string[0] == '\r') || (string[0] == '\n'))	//If this lyric begins with a newline or carraige return
		{
			EndLyricLine();			//End the lyric line before the lyric is added
			linebreaks=1;			//Track that line break character(s) were found in the lyrics
		}

		if(Lyrics.verbose >= 2)
		{
			string2=DuplicateString(string);	//Make a copy of the string for display purposes
			string2=TruncateString(string2,1);	//Remove leading/trailing whitespace, newline chars, etc.
			printf("Timestamp: 0x%X%X%X%X\t%lu %s\t\"%s\"\t%s",timestamparray[0],timestamparray[1],timestamparray[2],timestamparray[3],timestamp,(timestampformat == 1) ? "MPEG frames" : "Milliseconds",string2,(string[0]=='\n') ? "(newline)\n" : "\n");
			free(string2);
			string2=NULL;
		}

	//Perform grouping logic
		//Handle whitespace at the beginning of the parsed lyric piece as a signal that the piece will not group with previous piece
		if(isspace(string[0]))
			if(Lyrics.curline->pieces != NULL)	//If there was a previous lyric piece on this line
				Lyrics.lastpiece->groupswithnext=0;	//Ensure it is set to not group with this lyric piece

		if(isspace(string[strlen(string)-1]))	//If the lyric ends in a space
			groupswithnext=0;
		else
			groupswithnext=1;

		if(Lyrics.line_on == 0)		//Ensure that a line phrase is started
			CreateLyricLine();

	//Add lyric piece, during testing, I'll just write it with a duration of 1ms
		AddLyricPiece(string,timestamp,timestamp+1,PITCHLESS,groupswithnext);	//Write lyric with no defined pitch
		free(string);	//Free string

		if((Lyrics.lastpiece != NULL) && (Lyrics.lastpiece->prev != NULL) && (Lyrics.lastpiece->prev->groupswithnext))	//If this piece groups with the previous piece
			Lyrics.lastpiece->prev->duration=Lyrics.lastpiece->start-Lyrics.realoffset-Lyrics.lastpiece->prev->start;	//Extend previous piece's length to reach this piece, take the current offset into account
	}//While we haven't reached the end of the SYLT frame

//If the imported lyrics did not contain line breaks, they must be inserted manually
	if(!linebreaks && Lyrics.piececount)
	{
		if(Lyrics.verbose)	puts("\nImported ID3 lyrics did not contain line breaks, adding...");
		ptr=Lyrics.lines->pieces;
		lineptr=Lyrics.lines;	//Point to first line of lyrics (should be the only line)

		assert_wrapper((lineptr != NULL) && (ptr != NULL));	//This shouldn't be possible if Lyrics.piececount is nonzero

		if(lineptr->next != NULL)	//If there is another line of lyrics defined
			return;					//abort the insertion of automatic line breaks

		while((ptr != NULL) && (ptr->next != NULL))
		{
			ptr2=ptr->next;	//Store pointer to next lyric
			lineptr=InsertLyricLineBreak(lineptr,ptr2);	//Insert a line break between this lyric and the next, line conductor points to new line
			ptr=ptr2;		//lyric conductor points to the next lyric, which is at the beginning of the new line
		}
	}
}

unsigned long ID3FrameProcessor(struct ID3Tag *ptr)
{
	unsigned char header[10]={0};	//Used to store the ID3 frame header
	unsigned long filepos=0;
	int ctr2=0;					//Used to validate frame ID
	struct ID3Frame *temp=NULL;	//Used to allocate an ID3Frame link
	struct ID3Frame *head=NULL;	//Used as the head pointer
	struct ID3Frame *cond=NULL;	//Used as the conductor
	unsigned long ctr=0;		//Count the number of frames processed
	unsigned long framesize=0;	//Used to validate frame size

//Validate input parameter
	if((ptr == NULL) || (ptr->fp == NULL))
		return 0;	//Return failure

//Rewind the file
	rewind(ptr->fp);

//Find and parse the ID3 header
	if(FindID3Tag(ptr) == 0)	//Locate the ID3 tag
		return 0;				//Return failure

	if(Lyrics.verbose)	puts("Loading ID3 tag");

//Load ID3 frames into linked list
	fseek_err(ptr->fp,ptr->framestart,SEEK_SET);	//Seek to first ID3 frame
	filepos=ftell_err(ptr->fp);			//Record file position of first expected ID3 frame
	while((filepos >= ptr->framestart) && (filepos < ptr->tagend))
	{//While file position is at or after the end of the ID3 header, before or at end of the ID3 tag
		//Verify this is an ID3 frame (ValidateID3FrameHeader)
/*v2.3	Disuse ValidateID3FrameHeader() and validate the frame here instead
		if(!ValidateID3FrameHeader(ptr))	//If this is not a valid ID3 frame
		{
			c=fgetc(ptr->fp);			//Seek forward one byte
			filepos=ftell_err(ptr->fp);	//Record file position of first expected ID3 frame
			continue;					//and loop
		}
*/

	//Read frame header into buffer
		if(fread(header,10,1,ptr->fp) != 1)
			return 0;	//Return failure on I/O error

	//Validate frame ID
		for(ctr2=0;ctr2<4;ctr2++)
		{
			if(!isupper(header[ctr2]) && !isdigit(header[ctr2]))	//If the character is NOT valid for an ID3 frame ID
			{
				ptr->frames=head;	//Store the linked list into the ID3 tag structure
				return ctr;	//Return the number of frames that were already processed
			}
		}

	//Validate frame end
		framesize=((unsigned long)header[4]<<24) | ((unsigned long)header[5]<<16) | ((unsigned long)header[6]<<8) | ((unsigned long)header[7]);	//Convert to 4 byte integer
		if(filepos + framesize + 10 >= ptr->tagend)	//If the defined frame size would cause the frame to extend beyond the end of the ID3 tag
			return 0;	//Return failure

	//Initialize an ID3Frame structure
		temp=calloc_err(1,sizeof(struct ID3Frame));	//Allocate and init memory to 0
		temp->frameid=calloc_err(1,5);			//Allocate and init 5 bytes to 0 for the ID3 frame ID
		memcpy(temp->frameid,header,4);			//Copy the 4 byte ID3 frame ID into the pre-terminated string
		temp->pos=filepos;	//Record the file position of the ID3 frame
		temp->length=((unsigned long)header[4]<<24) | ((unsigned long)header[5]<<16) | ((unsigned long)header[6]<<8) | ((unsigned long)header[7]);
			//Record the frame length (defined in header as 4 byte Big Endian integer)

		//Append ID3Frame link to the list
		if(head == NULL)	//Empty list
		{
			head=temp;	//Point head to new link
			cond=temp;	//Point conductor to new link
		}
		else
		{
			assert_wrapper(cond != NULL);	//The linked list is expected to not be corrupted
			cond->next=temp;	//Conductor points forward to new link
			temp->prev=cond;	//New link points back to conductor
			cond=temp;		//Conductor points to new link
		}

		if(Lyrics.verbose >= 2)	printf("\tFrame ID %s loaded\n",temp->frameid);
		ctr++;	//Iterate counter

//v2.3	Implement seeking to next frame instead of seeking one byte at a time
//		filepos=ftell_err(ptr->fp);	//Record file position of the next potential ID3 frame
		fseek(ptr->fp,framesize,SEEK_CUR);	//Seek ahead to the beginning of the next ID3 frame
		filepos+=framesize + 10;	//Update file position
	}//While file position is at or after the end of the ID3 header, before or at end of the ID3 tag

	if(Lyrics.verbose)	printf("%lu ID3 frames loaded\n\n",ctr);

	ptr->frames=head;	//Store the linked list into the ID3 tag structure
	return ctr;			//Return the number of frames loaded
}

int ValidateID3FrameHeader(struct ID3Tag *ptr)
{
	unsigned long position=0;
	unsigned char buffer[10]={0};	//Used to read the frame header
	unsigned long ctr=0;
	unsigned long framesize=0;
	char success=1;	//Will assume success unless failure is detected

//Validate input parameters
	if((ptr == NULL) || (ptr->fp == NULL))
		return 0;	//Return failure

//Record file position
	position=ftell_err(ptr->fp);

//If file position is less than the start of the ID3 tag, return failure
	if(position < ptr->framestart)
		return 0;	//Return failure

//Read 10 bytes into buffer
	if(fread(buffer,10,1,ptr->fp) != 1)	//If there was a file I/O error
		success=0;	//Record failure

//Validate header:  Verify that each of the first four buffered characters are an uppercase letter or numerical character
	if(success)
		for(ctr=0;ctr<4;ctr++)
		{
			if(!isupper(buffer[ctr]) && !isdigit(buffer[ctr]))	//If the character is valid for an ID3 frame ID
			{
				success=0;	//Record failure
				break;
			}
		}

//Validate frame position:  Verify that the end of the frame occurs at or before the end of the ID3 tag
	if(success)
	{
		framesize=((unsigned long)buffer[4]<<24) | ((unsigned long)buffer[5]<<16) | ((unsigned long)buffer[6]<<8) | ((unsigned long)buffer[7]);	//Convert to 4 byte integer
		if(position + framesize + 10 >= ptr->tagend)	//If the defined frame size would cause the frame to extend beyond the end of the ID3 tag
			success=0;	//Record failure
	}

//Seek to original file position
	fseek_err(ptr->fp,position,SEEK_SET);

//Return failure/success status
	return success;	//Return success/failure status
}

void DestroyID3FrameList(struct ID3Tag *ptr)
{
	struct ID3Frame *temp=NULL,*temp2=NULL;

	if(ptr==NULL)
		return;

	for(temp=ptr->frames;temp!=NULL;temp=temp2)	//For each link
	{
		temp2=temp->next;	//Remember this pointer
		if(temp->frameid != NULL)
			free(temp->frameid);
		free(temp);
	}

	ptr->frames=NULL;
}

unsigned long BuildID3Tag(struct ID3Tag *ptr,FILE *outf)
{
	unsigned long tagpos=0;		//Records the position of the ID3 tag in the output file
	unsigned long tagend=0;		//Records the position of one byte past the end of the written ID3 tag
	unsigned long framepos=0;	//Records the position of the SYLT frame in the output file
	int c=0;
	unsigned long ctr=0;
	struct ID3Frame *temp=NULL;
	struct Lyric_Line *curline=NULL;	//Conductor of the lyric line linked list
	struct Lyric_Piece *curpiece=NULL;	//A conductor for the lyric pieces list
	unsigned long framesize=0;	//The calculated size of the written SYLT frame
	unsigned long tagsize=0;	//The calculated size of the modified ID3 tag
	unsigned char array[4]={0};	//Used to build the 28 bit ID3 tag size
	unsigned char defaultID3tag[10]={'I','D','3',3,0,0,0,0,0,0};
		//If the input file had no ID3 tag, this tag will be written
	unsigned long fileendpos=0;	//Used to store the size of the input file
	char newline=0;				//ID3 spec indicates that newline characters should be used at the beginning
								//of the new line instead of at the end of a line, use this to track for this

//Validate input parameters
	if((ptr == NULL) || (ptr->fp == NULL) || (outf == NULL))
		return 0;	//Return failure

	if(Lyrics.verbose)	printf("\nExporting ID3 lyrics to file \"%s\"\n",Lyrics.outfilename);

//Conditionally copy the existing ID3 tag from the source file, or create one from scratch
//v2.3	Implemented linked list of ID3 frames to omit
//	if((ptr->frames == NULL) || Lyrics.nosrctag)
	if(ptr->frames == NULL)
	{	//If there was no ID3 tag in the input file
		if(Lyrics.verbose)	puts("Writing new ID3 tag");
		tagpos=ftell_err(outf);	//Record this file position so the tag size can be rewritten later
		fwrite_err(defaultID3tag,10,1,outf);	//Write a pre-made ID3 tag
	}
	else
	{	//If there was an ID3 tag in the source file
		rewind(ptr->fp);

//If the ID3 header isn't at the start of the source file, copy all data that precedes it to the output file
		while((unsigned long)ftell_err(ptr->fp) < ptr->tagstart)
		{	//For each byte that preceeded the ID3 tag
			c=fgetc_err(ptr->fp);	//Read it from input file
			fputc_err(c,outf);	//Write it to the output file
		}

//Copy the original ID3 header from source file to output file (record the file position)
		if(Lyrics.verbose)	puts("Copying ID3 tag header");

		tagpos=ftell_err(outf);	//Record this file position so the tag size can be rewritten later
		while((unsigned long)ftell_err(ptr->fp) < ptr->framestart)
		{	//For each byte that preceeded the first ID3 frame (header information)
			c=fgetc_err(ptr->fp);	//Read it from input file
			fputc_err(c,outf);		//Write it to the output file
		}

//Write tag information from input file if applicable, and ensure that equivalent tags from the source file are omitted
		if(Lyrics.Title != NULL)
		{
			WriteTextInfoFrame(outf,"TIT2",Lyrics.Title);					//Write song title frame
			Lyrics.nosrctag=AddOmitID3framelist(Lyrics.nosrctag,"TIT2");	//Linked list create/append to omit source song title frame
		}
		if(Lyrics.Artist != NULL)
		{
			WriteTextInfoFrame(outf,"TPE1",Lyrics.Artist);					//Write song artist frame
			Lyrics.nosrctag=AddOmitID3framelist(Lyrics.nosrctag,"TPE1");	//Linked list create/append to omit source song artist frame
		}
		if(Lyrics.Album != NULL)
		{
			WriteTextInfoFrame(outf,"TALB",Lyrics.Album);					//Write album frame
			Lyrics.nosrctag=AddOmitID3framelist(Lyrics.nosrctag,"TALB");	//Linked list create/append to omit source album frame
		}

//Omit any existing SYLT frame from the source file
		Lyrics.nosrctag=AddOmitID3framelist(Lyrics.nosrctag,"SYLT");		//Linked list create/append with SYLT frame ID to ensure SYLT source frame is omitted

//Write all frames from source MP3 to export MP3 except for those in the omit list
		for(temp=ptr->frames;temp!=NULL;temp=temp->next)
		{	//For each ID3Frame in the list
//			if(strcmp(temp->frameid,"SYLT") != 0)	//If the frame is not a Synchronized Lyric frame
			if(SearchOmitID3framelist(Lyrics.nosrctag,temp->frameid) == 0)	//If the source frame isn't to be omitted
			{
				if(Lyrics.verbose >= 2)	printf("\tCopying frame \"%s\"\n",temp->frameid);

				if((unsigned long)ftell_err(ptr->fp) != temp->pos)	//If the input file isn't already positioned at the frame
					fseek_err(ptr->fp,temp->pos,SEEK_SET);			//Seek to it now
				while((unsigned long)ftell_err(ptr->fp) < temp->pos + temp->length + 10)
				{	//For each byte that is before the next frame's header
					c=fgetc_err(ptr->fp);	//Read it from input file
					fputc_err(c,outf);	//Write it to the output file
				}
				ctr++;	//Increment counter
			}
			else if(Lyrics.verbose >= 2)	printf("\tOmitting \"%s\" frame from source file\n",temp->frameid);
		}
	}//If there was an ID3 tag in the input file

	if(Lyrics.verbose)	puts("Writing SYLT frame header");

//Write SYLT frame header
	framepos=ftell_err(outf);	//Record this file position so the frame size can be rewritten later
	fputs_err("SYLTxxxx\xC0",outf);	//Write the Frame ID, 4 dummy bytes for the unknown frame size and the first flag byte (preserve frame for both tag and file alteration)
	fputc_err(0,outf);		//Write second flag byte (no compression, encryption or grouping identity)
	fputc_err(0,outf);		//ASCII encoding
	fputs_err("eng\x02\x01",outf);	//Write the language as "English", the timestamp format as milliseconds and the content type as "lyrics"
	fputs_err(PROGVERSION,outf);	//Embed the program version as the content descriptor
	fputc_err(0,outf);		//Write NULL terminator for content descriptor

//Write SYLT frame using the Lyrics structure
	curline=Lyrics.lines;	//Point lyric line conductor to first line of lyrics

	if(Lyrics.verbose)	puts("Writing SYLT lyrics");

	while(curline != NULL)	//For each line of lyrics
	{
		curpiece=curline->pieces;	//Starting with the first piece of lyric in this line
		while(curpiece != NULL)		//For each piece of lyric in this line
		{
			if(newline)
			{
				fputc_err('\n',outf);	//Append a newline character
				newline=0;				//Reset this status
			}
			fputs_err(curpiece->lyric,outf);//Write the lyric
			if(Lyrics.verbose >= 2)	printf("\t\"%s\"\tstart=%lu\t",curpiece->lyric,curpiece->start);

//Line/word grouping logic
			if(curpiece->next == NULL)		//If this is the last lyric in the line
			{
				newline=1;
				if(Lyrics.verbose >= 2) printf("(newline)");
			}
			else if(!curpiece->groupswithnext)	//Otherwise, if this lyric does not group with the next
			{
				fputc_err(' ',outf);		//Append a space
				if(Lyrics.verbose >= 2)	printf("(whitespace)");
			}
			if(Lyrics.verbose >= 2)	putchar('\n');

			fputc_err(0,outf);				//Write a NULL terminator
			WriteDWORDBE(outf,curpiece->start);	//Write the lyric's timestamp as a big endian value
			curpiece=curpiece->next;		//Point to next lyric in the line
		}
		curline=curline->next;				//Point to next line of lyrics
	}

	framesize=ftell_err(outf)-framepos-10;	//Find the length of the SYLT frame that was written (minus frame header size)
	ctr++;	//Increment counter

	if(ptr->frames != NULL)
	{	//If the input MP3 had an ID3 tag
//Copy any unidentified data (ie. padding) that occurs between the end of the last ID3 frame in the list and the defined end of the ID3 tag
		for(temp=ptr->frames;temp->next!=NULL;temp=temp->next);		//Seek to last frame link
		fseek_err(ptr->fp,temp->pos + temp->length + 10,SEEK_SET);	//Seek to one byte past the end of the frame
		while((unsigned long)ftell_err(ptr->fp) < ptr->tagend)
		{	//For each byte until the outside of the ID3 tag is reached
			c=fgetc_err(ptr->fp);	//Read it from input file
			fputc_err(c,outf);		//Write it to the output file
		}
		tagend=ftell_err(ptr->fp);	//Record the file position of the end of the tag
	}

	tagsize=ftell_err(outf)-tagpos-10;	//Find the length of the ID3 tag that has been written (minus tag header size)

	if(Lyrics.verbose)	puts("Copying audio data");

	fileendpos=GetFileEndPos(ptr->fp);	//Find the position of the last byte in the input MP3 file (the filesize)
	switch(BlockCopy(ptr->fp,outf,fileendpos-ftell_err(ptr->fp)))
	{
		case -1:	//If BlockCopy() couldn't allocate enough memory to block read+write the remaining data
		//Manually copy all bytes one at a time from input file to output file until EOF is encountered
			puts("Block copy failed, performing slow copy");
			while(!feof(ptr->fp))
			{	//Until end of input file is reached
				c=fgetc(ptr->fp);	//Read one byte from input file
				if(c != EOF)		//If the read was successful
					fputc_err(c,outf);	//Write it to the output file
			}
			break;

		case -2:	//File I/O error
			puts("Error copying data from input to output file\nAborting");
			exit_wrapper(1);
			break;

		default:	//Success
		break;
	}

	if(Lyrics.verbose)	puts("Correcting ID3 headers");

//Rewind to the SYLT header in the output file and write the correct frame length
	fseek_err(outf,framepos+4,SEEK_SET);	//Seek to where the SYLT frame size is to be written
	WriteDWORDBE(outf,framesize);			//Write the SYLT frame size

//Rewind to the ID3 header in the output file and write the correct ID3 tag length
	fseek_err(outf,ptr->tagstart+6,SEEK_SET);	//Seek to where the ID3 tag size is to be written
	if(tagsize > 0x0FFFFFFF)
	{
		puts("\aError:  Tag size is larger than the ID3 specification allows");
		exit_wrapper(2);
	}

	array[3]=tagsize & 127;			//Mask out everything except the lowest 7 bits
	array[2]=(tagsize>>7) & 127;	//Mask out everything except the 2nd set of 7 bits
	array[1]=(tagsize>>14) & 127;	//Mask out everything except the 3rd set of 7 bits
	array[0]=(tagsize>>21) & 127;	//Mask out everything except the 4th set of 7 bits

	fwrite_err(array,4,1,outf);		//Write the ID3 tag size

	if(Lyrics.verbose)	printf("\nID3 export complete.  %lu lyrics written\n",Lyrics.piececount);

	return ctr;	//Return counter
}

void Export_ID3(FILE *inf, FILE *outf)
{
	struct ID3Tag tag={NULL,0,0,0,0,0,0.0,NULL};

//Ensure the necessary information is available
	assert_wrapper((inf != NULL) && (outf != NULL));

	tag.fp=inf;

//Seek to first MPEG frame in input file (after ID3 tag, otherwise presume it's at the beginning of the file)
	if(Lyrics.verbose)	puts("Parsing input MPEG audio file");
	if(FindID3Tag(&tag))		//Find start and end of ID3 tag
		fseek_err(tag.fp,tag.tagend,SEEK_SET);	//Seek to the first MP3 frame (immediately after the ID3 tag)
	else
		rewind(tag.fp);	//Rewind file if no ID3 tag is found

//Load MPEG information
	if(GetMP3FrameDuration(&tag) == 0)	//Find the sample rate defined in the MP3 frame
	{
		printf("Error loading MPEG information from file \"%s\"\nAborting",Lyrics.srcfile);
		exit_wrapper(1);
	}

//Load any existing ID3 frames
	ID3FrameProcessor(&tag);

//Build output file
	BuildID3Tag(&tag,outf);

//Release memory
	DestroyID3FrameList(&tag);	//Release ID3 frame linked list
	DestroyOmitID3framelist(Lyrics.nosrctag);	//Release ID3 frame omission list
}

struct ID3Frame *FindID3Frame(struct ID3Tag *tag,const char *frameid)
{
	struct ID3Frame *ptr;

//Validate parameters
	assert_wrapper((tag != NULL) && (tag->fp != NULL) && (tag->frames != NULL) && (frameid != NULL));

//Locate the specified frame
	for(ptr=tag->frames;ptr != NULL;ptr=ptr->next)		//For each frame in the linked list
		if(strcasecmp(ptr->frameid,frameid) == 0)	//If this is the frame being searched for
			return ptr;				//Return success

	return NULL;	//If no result was found, return failure
}

char *GrabID3TextFrame(struct ID3Tag *tag,const char *frameid,char *buffer,unsigned long buffersize)
{
	struct ID3Frame *frameptr=NULL;
	char *string=NULL;

//Validate parameters
	assert_wrapper((tag != NULL) && (tag->fp != NULL) && (tag->frames != NULL) && (frameid != NULL));
	if(buffer != NULL)
		assert_wrapper(buffersize != 0);

//Locate the specified frame
	frameptr=FindID3Frame(tag,frameid);
	if(frameptr == NULL)	//If no match was found
		return NULL;		//return without altering the buffer

//Seek to the frame's location in the file
	if(fseek(tag->fp,frameptr->pos,SEEK_SET) != 0)	//If there was a file I/O error seeking
		return NULL;		//return without altering the buffer

//Read the text info string
	string=ReadTextInfoFrame(tag->fp);
	if(string == NULL)	//If there was an error reading the string
		return NULL;		//return without altering the buffer

//If performing the logic to buffer the string
	if(buffer != NULL)
	{
	//Verify string will fit in buffer
		if(strlen(string) + 1 > buffersize)	//If the string (including NULL terminator) is larger than the buffer
			string[buffersize-1]='\0';	//truncate the string to fit

		strcpy(buffer,string);	//Copy string to buffer
		free(string);		//Release memory used by the string
		return buffer;		//Return success
	}

//Otherwise return the allocated string
	return string;
}

struct OmitID3frame *AddOmitID3framelist(struct OmitID3frame *ptr,const char *frameid)
{
	struct OmitID3frame *head=NULL;
	struct OmitID3frame *temp=NULL;

	assert_wrapper(frameid != NULL);
	head=ptr;	//Save this address

//Allocate and init new link
	temp=malloc_err(sizeof(struct OmitID3frame));
	temp->frameid=DuplicateString(frameid);
	temp->next=NULL;

//Append link to existing list if applicable
	if(ptr != NULL)
	{
		while(ptr->next != NULL)	//Seek to last link in the list
			ptr=ptr->next;

		ptr->next=temp;	//Last link points forward to new link
	}

	if(head != NULL)	//If this linked list already had a head
		return head;	//return it
	else
		return temp;	//Otherwise return the new linked list head
}

int SearchOmitID3framelist(struct OmitID3frame *ptr,char *frameid)
{
	assert_wrapper(frameid != NULL);
	while(ptr != NULL)
	{
		assert_wrapper(ptr->frameid != NULL);
		if(ptr->frameid[0]=='*')
			return 2;	//Return wildcard match
		if(strcasecmp(ptr->frameid,frameid) == 0)
			return 1;	//Return exact match

		ptr=ptr->next;
	}

	return 0;	//Return no match
}

void DestroyOmitID3framelist(struct OmitID3frame *ptr)
{
	struct OmitID3frame *temp;

	while(ptr != NULL)
	{
		if(ptr->frameid != NULL)
			free(ptr->frameid);

		temp=ptr->next;	//Save address to next link
		free(ptr);	//Free this link
		ptr=temp;	//Point to next link
	}
}

void WriteTextInfoFrame(FILE *outf,const char *frameid,const char *string)
{
	unsigned long size=0;

//Validate input parameters
	assert_wrapper((outf != NULL) && (frameid != NULL) && (string != NULL));
	if(strlen(frameid) != 4)
	{
		printf("Error: Attempted export of invalid frame ID \"%s\"\nAborting",frameid);
		exit_wrapper(1);
	}

	if(Lyrics.verbose >= 2)	printf("\tWriting frame \"%s\"\n",frameid);

	size=strlen(string)+1;		//The frame payload size is the string and the encoding byte

//Write ID3 frame header
	fwrite_err(frameid,4,1,outf);	//Write frame ID
	WriteDWORDBE(outf,size);	//Write frame size (total frame size-header size) in Big Endian format
	fputc_err(0xC0,outf);		//Write first flag byte (preserve frame for both tag and file alteration)
	fputc_err(0,outf);		//Write second flag byte (no compression, encryption or grouping identity)

//Write frame data
	fputc_err(0,outf);			//Write ASCII encoding
	fwrite_err(string,size-1,1,outf);	//Write the string
}
