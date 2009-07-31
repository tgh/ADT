/*
 * Copyright © 2009 Tyler Hayes
 * ALL RIGHTS RESERVED
 * [This program is licensed under the GPL version 3 or later.]
 * Please see the file COPYING in the source
 * distribution of this software for license terms.
 *
 * XXX
 *
 * Thanks to:
 * - Bart Massey of Portland State University (http://web.cecs.pdx.edu/~bart/)
 *   for suggesting LADSPA plugins as a project.
 */


//----------------
//-- INCLUSIONS --
//----------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ladspa.h"


//-----------------------
//-- DEFINED CONSTANTS --
//-----------------------
/*
 * These are the port numbers for the plugin
 */
// left channel input
#define ADT_INPUT_LEFT 0
// right channel output
#define ADT_INPUT_RIGHT 1
// left channel input
#define ADT_OUTPUT_LEFT 2
// right channel output
#define ADT_OUTPUT_RIGHT 3

/*
 * Other constants
 */
// the plugin's unique ID given by Richard Furse (ladspa@muse.demon.co.uk)
#define UNIQUE_ID 4305
// number of ports involved
#define PORT_COUNT 4
// the number of milliseconds for the offset
#define OFFSET_IN_MILLISECONDS 5


//-------------------------
//-- FUNCTION PROTOTYPES --
//-------------------------

// gets the offset value in samples
int GetOffsetInSamples(LADSPA_Data sample_rate);


//--------------------------------
//-- STRUCT FOR PORT CONNECTION --
//--------------------------------

typedef struct {
	
	// a buffer to hold the samples at the end of the right channel input
	// buffer that will be left out due to the offset of the output buffer
	LADSPA_Data * block_run_off;
	
	// the sample rate of the audio
	LADSPA_Data sample_rate;
	
	// data locations for the input & output audio ports
	LADSPA_Data * Input_Left;
	LADSPA_Data * Input_Right;
	LADSPA_Data * Output_Left;
	LADSPA_Data * Output_Right;
} Adt;


//---------------
//-- FUNCTIONS --
//---------------

/*
 * Creates a plugin instance by allocating space for a plugin handle.
 * This function returns a LADSPA_Handle (which is a void * -- a pointer to
 * anything).
 */
LADSPA_Handle instantiate_Adt(const LADSPA_Descriptor * Descriptor,
										unsigned long sample_rate)
{
	printf("\nIn instantiate_Adt");
	
	Adt * adt;
	// allocate space for a Adt struct instance
	adt = (Adt *) malloc(sizeof(Adt));
	// make sure malloc was a success
	if (adt)
	{
		// get the offest in samples
		int sample_offset = GetOffsetInSamples((LADSPA_Data)sample_rate);
		// allocate space for the samples from the input buffer that will be left out
		adt->block_run_off = malloc(sizeof(LADSPA_Data) * sample_offset);
		// return NULL if malloc failed
		if (!adt->block_run_off)
		{
			free(adt);
			return NULL;
		}
		// set the sample rate
		adt->sample_rate = (LADSPA_Data)sample_rate;
	}
	// send the LADSPA_Handle to the host.  If malloc failed, NULL is returned.
	return adt;
}

//-----------------------------------------------------------------------------

/*
 * This procedure just zero's out the block run off buffer inside the Adt
 * struct that's allocated in instantiate_Adt.  The other pointers inside
 * Adt are set to memory allocated by the host, which is why there are not
 * regarded here--only the ones allocated in instantiate_Adt, which is
 * block_run_off.
 */
void activate_Adt(LADSPA_Handle instance)
{
	printf("\nIn activate_Adt");
	
	Adt * adt = (Adt *) instance;
	// get the offset in samples in order to send the size of the buffer to memset
	int sample_offset = GetOffsetInSamples(adt->sample_rate);
	/*
	 * Richard Furse did this in his simple delay line plugin (part of the
	 * ladspa SDK).  Here is his comment: "Need to reset the delay history
	 * in this function rather than instantiate() in case deactivate()
	 * followed by activate() have been called to reinitialise a delay line."
	 * Of course, this isn't delay, but the idea is the same.  However, I
	 * really don't understand it, seeing as neither that plugin nor this one
	 * have a deactivate() function...
	 */
	memset(adt->block_run_off, 0, sizeof(LADSPA_Data) * sample_offset);
}

//-----------------------------------------------------------------------------

/*
 * Make a connection between a specified port and it's corresponding data location.
 * For example, the output port should be "connected" to the place in memory where
 * that sound data to be played is located.
 */
void connect_port_to_Adt(LADSPA_Handle instance, unsigned long Port, LADSPA_Data * data_location)
{
	printf("\nIn connect_port_to_Adt");
	
	Adt * adt;
	
	// cast the (void *) instance to (Adt *) and set it to local pointer
	adt = (Adt *) instance;
	
	// direct the appropriate data pointer to the appropriate data location
	switch (Port)
	{
		case ADT_INPUT_LEFT :
			adt->Input_Left = data_location;
			break;
		case ADT_INPUT_RIGHT :
			adt->Input_Right = data_location;
			break;
		case ADT_OUTPUT_LEFT :
			adt->Output_Left = data_location;
			break;
		case ADT_OUTPUT_RIGHT :
			adt->Output_Right = data_location;
			break;	
	}
}

//-----------------------------------------------------------------------------

/*
 * Here is where the rubber hits the road.  The actual sound manipulation
 * is done in run().
 * XXX
 */
void run_Adt(LADSPA_Handle instance, unsigned long total_samples)
{
	printf("\nIn run_Adt");

	Adt * adt = (Adt *) instance;

	/*
	 * NOTE: these special cases should never happen, but you never know--like
	 * if someone is developing a host program and it has some bugs in it, it
	 * might pass some bad data.  If that's the case these printf's will help
	 * the developer anyway!
	 */
	if (total_samples <= 1)
	{
		printf("\nA sample count of 0 or 1 was sent to plugin.");
		printf("\nPlugin not executed.\n");
		return;
	}
	if (!adt)
	{
		printf("\nPlugin received NULL pointer for plugin instance.");
		printf("\nPlugin not executed.\n");
		return;
	}
	// for a one millisecond offest, a sample rate of 1000 is as low as you would
	// want to go.  Anything below that would risk having an offset of 0 samples,
	// which is pointless.
	if (adt->sample_rate < 1000.0f)
	{
		printf("\nPlugin received a sample rate below 100 samples per second.");
		printf("\nPlugin not executed.\n");
		return;
	}
	
	// get the offset in samples
	const int SAMPLE_OFFSET = GetOffsetInSamples(adt->sample_rate);
	
	// buffer pointers
	LADSPA_Data * input = NULL;
	LADSPA_Data * output = NULL;
	LADSPA_Data * run_off = adt->block_run_off;
	
	// buffer indexes
	unsigned long in_index = 0;
	unsigned long out_index = 0;
	int run_off_index = 0;
	
	// copy all left channel input buffer into left channel output buffer
	input = adt->Input_Left;
	output = adt->Output_Left;
	for (in_index = 0; in_index < total_samples; ++in_index)
	{
		output[out_index] = input[in_index];
		++out_index;
	}
	// copy all samples from the run off buffer into the right channel output buffer
	output = adt->Output_Right;
	out_index = 0;
	for (run_off_index = 0; run_off_index < SAMPLE_OFFSET; ++run_off_index)
	{
		output[out_index] = run_off[run_off_index];
		++out_index;
	}
	// copy right channel input buffer into the rest of right channel output buffer
	input = adt->Input_Right;
	in_index = 0;
	for (out_index; out_index < total_samples; ++out_index)
	{
		output[out_index] = input[in_index];
		++in_index;
	}
	// copy the left over samples from the right channel input buffer into the
	// run off buffer
	run_off_index = 0;
	for (in_index; in_index < total_samples; ++in_index)
	{
		run_off[run_off_index] = input[in_index];
		++run_off_index;
	}
}

//-----------------------------------------------------------------------------

/*
 * Frees dynamic memory associated with the plugin instance.  The host
 * better send the right pointer in or there's gonna be a leak!
 */
void cleanup_Adt(LADSPA_Handle instance)
{
	printf("\nIn cleanup_Adt");

	Adt * adt = (Adt *) instance;
	
	if (!adt)
		return;
	
	if (adt->block_run_off)
		free (adt->block_run_off);
	free (adt);
}

//-----------------------------------------------------------------------------

/*
 * Global LADSPA_Descriptor variable used in _init(), ladspa_descriptor(),
 * and _fini().
 */
LADSPA_Descriptor * Adt_descriptor = NULL;


/*
 * The _init() procedure is called whenever this plugin is first loaded
 * by the host using it (when the host program is first opened).
 */
void _init()
{
	printf("\nIn _init");

	/*
	 * allocate memory for Adt_descriptor (it's just a pointer at this point).
	 * in other words create an actual LADSPA_Descriptor struct instance that
	 * Adt_descriptor will point to.
	 */
	Adt_descriptor = (LADSPA_Descriptor *) malloc(sizeof(LADSPA_Descriptor));
	
	// make sure malloc worked properly before initializing the struct fields
	if (Adt_descriptor)
	{
		// assign the unique ID of the plugin given by Richard Furse
		Adt_descriptor->UniqueID = UNIQUE_ID;
		
		/*
		 * assign the label of the plugin.
		 * NOTE: it must not have white spaces as per ladspa.h.
		 * NOTE: in case you were wondering, strdup() from the string library makes a duplicate
		 * string of the argument and returns the duplicate's pointer (a char *).
		 */
		Adt_descriptor->Label = strdup("ADT");
		
		/*
		 * assign the special property of the plugin, which is any of the three
		 * defined in ladspa.h: LADSPA_PROPERTY_REALTIME, LADSPA_PROPERTY_INPLACE_BROKEN,
		 * and LADSPA_PROPERTY_HARD_RT_CAPABLE.  They are just ints (1, 2, and 4,
		 * respectively).  See ladspa.h for what they actually mean.
		 */
		Adt_descriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;
		
		// assign the plugin name
		Adt_descriptor->Name = strdup("ADT (Artificial Double Tracking)");
		
		// assign the author of the plugin
		Adt_descriptor->Maker = strdup("Tyler Hayes (tgh@pdx.edu)");
		
		/*
		 * assign the copyright info of the plugin (NOTE: use "None" for no copyright
		 * as per ladspa.h)
		 */
		Adt_descriptor->Copyright = strdup("GPL");
		
		/*
		 * assign the number of ports for the plugin.
		 */
		Adt_descriptor->PortCount = PORT_COUNT;
		
		LADSPA_PortDescriptor * temp_descriptor_array;	// used for allocating and initailizing a
																		// LADSPA_PortDescriptor array (which is
																		// an array of ints) since Adt_descriptor->
																		// PortDescriptors is a const *.
		
		// allocate space for the temporary array with a length of the number of ports (PortCount)
		temp_descriptor_array = (LADSPA_PortDescriptor *) calloc(PORT_COUNT, sizeof(LADSPA_PortDescriptor));
		
		/*
		 * set the instance LADSPA_PortDescriptor array (PortDescriptors) pointer to
		 * the location temp_descriptor_array is pointing at.
		 */
		Adt_descriptor->PortDescriptors = (const LADSPA_PortDescriptor *) temp_descriptor_array;
		
		/*
		 * set the port properties by ORing specific bit masks defined in ladspa.h.
		 *
		 * these first two give the input ports the properties that tell the host that
		 * the ports takes input and are audio ports (not control ports).
		 */
		temp_descriptor_array[ADT_INPUT_LEFT] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		temp_descriptor_array[ADT_INPUT_RIGHT] = LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO;
		
		/*
		 * this gives the output ports the properties that tell the host that these ports
		 * are output ports and that they are audio ports (I don't see any situation where
		 * one might be an output port but not an audio port...).
		 */
		temp_descriptor_array[ADT_OUTPUT_LEFT] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		temp_descriptor_array[ADT_OUTPUT_RIGHT] = LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO;
		
		/*
		 * set temp_descriptor_array to NULL for housekeeping--we don't need that local
		 * variable anymore.
		 */
		temp_descriptor_array = NULL;
		
		char ** temp_port_names;	// temporary local variable (which is a pointer to an array
											// of arrays of characters) for the names of the ports since
											// Adt_descriptor->PortNames is a const char * const *.

		// allocate the space for two port names
		temp_port_names = (char **) calloc(PORT_COUNT, sizeof(char *));
		
		/*
		 * set the instance PortNames array pointer to the location temp_port_names
		 * is pointing at.
		 */
		Adt_descriptor->PortNames = (const char **) temp_port_names;
		
		// set the name of the input ports
		temp_port_names[ADT_INPUT_LEFT] = strdup("Input Left Channel");
		temp_port_names[ADT_INPUT_RIGHT] = strdup("Input Right Channel");
		
		// set the name of the ouput ports
		temp_port_names[ADT_OUTPUT_LEFT] = strdup("Output Left Channel");
		temp_port_names[ADT_OUTPUT_RIGHT] = strdup("Output Right Channel");
		
		// reset temp variable to NULL for housekeeping
		temp_port_names = NULL;
		
		LADSPA_PortRangeHint * temp_hints;	// temporary local variable (pointer to a
														// PortRangeHint struct) since Adt_descriptor->
														// PortRangeHints is a const *.
		
		// allocate space for two port hints (see ladspa.h for info on 'hints')									
		temp_hints = (LADSPA_PortRangeHint *) calloc(PORT_COUNT, sizeof(LADSPA_PortRangeHint));
		
		/*
		 * set the instance PortRangeHints pointer to the location temp_hints
		 * is pointed at.
		 */
		Adt_descriptor->PortRangeHints = (const LADSPA_PortRangeHint *) temp_hints;
		
		/*
		 * set the port hint descriptors (which are ints).
		 */
		temp_hints[ADT_INPUT_LEFT].HintDescriptor = 0;
		temp_hints[ADT_INPUT_RIGHT].HintDescriptor = 0;
		temp_hints[ADT_OUTPUT_LEFT].HintDescriptor = 0;
		temp_hints[ADT_OUTPUT_RIGHT].HintDescriptor = 0;
		
		// reset temp variable to NULL for housekeeping
		temp_hints = NULL;
		
		// set the instance's function pointers to appropriate functions
		Adt_descriptor->instantiate = instantiate_Adt;
		Adt_descriptor->connect_port = connect_port_to_Adt;
		Adt_descriptor->activate = activate_Adt;
		Adt_descriptor->run = run_Adt;
		Adt_descriptor->run_adding = NULL;
		Adt_descriptor->set_run_adding_gain = NULL;
		Adt_descriptor->deactivate = NULL;
		Adt_descriptor->cleanup = cleanup_Adt;
	}
}

//-----------------------------------------------------------------------------

/*
 * Returns a descriptor of this plugin.
 *
 * NOTE: this function MUST be called 'ladspa_descriptor' or else the plugin
 * will not be recognized.
 */
const LADSPA_Descriptor * ladspa_descriptor(unsigned long index)
{
	printf("\nIn ladspa_descriptor");

	if (index == 0)
		return Adt_descriptor;
	else
		return NULL;
}

//-----------------------------------------------------------------------------

/*
 * This is called automatically when the host quits (when this dynamic library
 * is unloaded).  It frees all dynamically allocated memory associated with
 * the descriptor.
 */
void _fini()
{
	printf("\nIn _fini\n");

	if (Adt_descriptor)
	{
		free((char *) Adt_descriptor->Label);
		free((char *) Adt_descriptor->Name);
		free((char *) Adt_descriptor->Maker);
		free((char *) Adt_descriptor->Copyright);
		free((LADSPA_PortDescriptor *) Adt_descriptor->PortDescriptors);
		
		/*
		 * the for loop here is kind of unnecessary since the number of ports
		 * was hard coded for this plugin as 2, but whatever.
		 */
		int i = 0;
		for(i = 0; i < Adt_descriptor->PortCount; ++i)
			free((char *)(Adt_descriptor->PortNames[i]));
		
		free((char **) Adt_descriptor->PortNames);
		free((LADSPA_PortRangeHint *) Adt_descriptor->PortRangeHints);
		
		free(Adt_descriptor);
	}
}

//-----------------------------------------------------------------------------

/*
 * This function just converts the #defined offset from milliseconds to samples.
 */
int GetOffsetInSamples(LADSPA_Data sample_rate)
{
	// convert the offset in milliseconds to seconds
	LADSPA_Data offset_seconds = (LADSPA_Data)OFFSET_IN_MILLISECONDS / 1000.0f;
	// convert the seconds to samples
	int offset_samples = (int) (sample_rate * offset_seconds);
	
	return offset_samples;
}

// ------------------------------- EOF ----------------------------------------
