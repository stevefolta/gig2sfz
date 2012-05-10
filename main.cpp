#include "gig.h"
#include <string>
#include <fstream>
#include <math.h>

using namespace std;


void print_usage() {
	cout << "Usage:" << endl;
	cout << "  gig2sfz <file>" << endl;
	cout << endl;
	cout << "(Use gigextract to extract the samples.)" << endl;
	}

string out_file_name(string orig_name) {
	// Oops, not used, we'll use the instrument names.
	string result = orig_name;
	return result.replace(result.rfind(".gig"), 4, ".sfz");
	}


extern void convert_gig_file(gig::File* gig);
extern void convert_gig_instrument(gig::Instrument* instrument);
extern void convert_gig_region(gig::Region* region, ostream& sfz);
extern int gig_fine_tune_to_cents(uint32_t gig_fine_tune);


int main(int argc, char* argv[]) {
	if (argc != 2) {
		print_usage();
		return 1;
		}

	try {
		RIFF::File riff(argv[1]);
		gig::File gig(&riff);
		convert_gig_file(&gig);
		}
	catch (RIFF::Exception exception) {
		exception.PrintMessage();
		return 1;
		}

	return 0;
	}


void convert_gig_file(gig::File* gig)
{
	gig::Instrument* instrument = gig->GetFirstInstrument();
	for (; instrument; instrument = gig->GetNextInstrument())
		convert_gig_instrument(instrument);
}


void convert_gig_instrument(gig::Instrument* instrument)
{
	string name = instrument->pInfo->Name;
	if (name == "")
		name = "Unnamed Instrument";

	ofstream sfz((name + ".sfz").c_str());

	gig::Region* region = instrument->GetFirstRegion();
	for (; region; region = instrument->GetNextRegion())
		convert_gig_region(region, sfz);
}


void convert_gig_region(gig::Region* region, ostream& sfz)
{
	if (region->Dimensions > 0)
		sfz << "<group>" << endl;
	else {
		sfz << "<region>" << endl;
		gig::Sample* sample = region->GetSample();
		sfz << "sample=" << sample->pInfo->Name << endl;
		sfz << "pitch_keycenter=" << sample->MIDIUnityNote << endl;
		if (sample->FineTune)
			sfz << "tune=" << gig_fine_tune_to_cents(sample->FineTune) << endl;
		}
	sfz << "lokey=" << region->KeyRange.low << endl;
	sfz << "hikey=" << region->KeyRange.high << endl;
	sfz << endl;

	if (region->Dimensions > 0) {
		// Get dimension info.
		struct Dimension {
			gig::dimension_t	type;
			int top_bit, bits;
			uint8_t mask;
			};
		int num_dimensions = region->Dimensions;
		Dimension dimensions[num_dimensions];
		int start_bit = 0;
		for (int dim = 0; dim < num_dimensions; ++dim) {
			gig::dimension_def_t* dim_def = &region->pDimensionDefinitions[dim];
			switch (dim_def->dimension) {
				case gig::dimension_velocity:
				case gig::dimension_releasetrigger:
					// These are okay.
					dimensions[dim].type = dim_def->dimension;
					dimensions[dim].top_bit = start_bit + dim_def->bits - 1;
					dimensions[dim].bits = dim_def->bits;
					dimensions[dim].mask = 0;
					for (int which_bit = 0; which_bit < dim_def->bits; ++which_bit) {
						dimensions[dim].mask |= 0x01 << start_bit;
						start_bit += 1;
						}
					break;

				default:
					throw RIFF::Exception("Unsupported dimension type.");
					break;
				}
			}

		// Output the dimension regions.
		for (int which_dim_rgn = 0; which_dim_rgn < region->DimensionRegions; ++which_dim_rgn) {
			gig::DimensionRegion* dim_rgn = region->pDimensionRegions[which_dim_rgn];
			sfz << "<region>" << endl;
			sfz << "sample=" << dim_rgn->pSample->pInfo->Name << ".wav" << endl;
			sfz << "pitch_keycenter=" << (int) dim_rgn->UnityNote << endl;
			if (dim_rgn->FineTune)
				sfz << "tune=" << gig_fine_tune_to_cents(dim_rgn->FineTune) << endl;
			if (dim_rgn->Gain)
				sfz << "volume=" << (-dim_rgn->Gain / 655360.0) << endl;
			if (dim_rgn->SampleLoops > 0) {
				DLS::sample_loop_t* loop = &dim_rgn->pSampleLoops[0];
				sfz << "loop_mode=loop_continuous ";
				sfz << "loop_start=" << loop->LoopStart << " ";
				sfz << "loop_end=" << loop->LoopStart + loop->LoopLength - 1 << endl;
				}
			for (int dim = 0; dim < num_dimensions; ++dim) {
				Dimension* dimension = &dimensions[dim];
				switch (dimension->type) {
					case gig::dimension_velocity:
						{
							int loval =
								(which_dim_rgn & dimension->mask) << (6 - dimension->top_bit);
							sfz << "lovel=" << loval << " ";
							int range = (0x80 >> dimension->bits) - 1;
							sfz << "hivel=" << loval + range << endl;
						}
						break;

					case gig::dimension_releasetrigger:
						if (which_dim_rgn & dimension->mask)
							sfz << "trigger=release" << endl;
						break;
					}
				}
			sfz << endl;
			}

		sfz << endl;
		}
}


int gig_fine_tune_to_cents(uint32_t gig_fine_tune)
{
	// gig.h says this:
	//  	Specifies the fraction of a semitone up from the specified MIDI unity
	//  	note field.  A value of 0x80000000 means 1/2 semitone (50 cents) and a
	//  	value of 0x00000000 means no fine tuning between semitones.
	// But it seems to really be integer cents.

	// return round(((double) gig_fine_tune / (double) 0x80000000) * 50);
	return (int) gig_fine_tune;
}



