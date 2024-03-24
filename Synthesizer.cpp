#include "olcNoiseMaker.hpp"

#include <iostream>
#include <algorithm>
#include <list>

using namespace std;

namespace synth
{
    //atomic<double> frequencyOutput = 0.0;
    //double octaveBaseFrequency = 131.0; // A2 sound = 110Hz // C4 sound = 262Hz // C2 sound = 65Hz
    //double const12thRootOf2 = pow(2.0, 1.0 / 12.0); // as 12 notes per octave

    // convert frequency (Hz) to angular velocity (omega = w)
    double angularVelocity(double hertz) {
        return hertz * 2.0 * PI;
    }

    const int OSC_SINE = 0;
    const int OSC_SQUARE = 1;
    const int OSC_TRIANGLE = 2;
    const int OSC_SAW_ANALOGUE = 3;
    const int OSC_SAW_DIGITAL = 4;
    const int OSC_NOISE = 5;

    double oscillator(double time, double hertz, int type = OSC_SINE, double lowFrequencyOscillatorHertz = 0.0, double lowFrequencyOscillatorAmplitude = 0.0, double custom = 50.0) {

        double frequency = angularVelocity(hertz) * time + lowFrequencyOscillatorAmplitude * hertz * (sin(angularVelocity(lowFrequencyOscillatorHertz) * time));

        switch (type) {
            // sine wave
        case OSC_SINE:
            return sin(frequency);

            // square wave
        case OSC_SQUARE:
            // if then else syntax
            return sin(frequency) > 0.0 ? 1.0 : -1.0;

            // triangle wave
        case OSC_TRIANGLE:
            return asin(sin(frequency)) * (2.0 / PI);

            // saw wave analogue
        case OSC_SAW_ANALOGUE:
        {
            double output = 0.0;

            for (double n = 1.0; n < 100.0; n++) {
                output += (sin(n * frequency)) / n;
            }

            return output * (2.0 * PI);
        }

        // saw wave optimised
        case OSC_SAW_DIGITAL:
            // FIX THIS TO ADD LFOfreq
            return (2.0 * PI) * (hertz * PI * fmod(time, 1.0 / hertz) - (PI / 2.0));

            // pseudo random noise
        case OSC_NOISE:
            return 2.0 * ((double)rand() / (double)RAND_MAX) - 1.0;

        default:
            return 0;
        }
    }

    struct note {
        int id;     // position in scale
        double timeOn;
        double timeOff;
        bool isActive;
        int channel;    // voice

        note() {
            id = 0;
            timeOn = 0.0;
            timeOff = 0.0;
            isActive = false;
            channel = 0;
        }
    };

    const int SCALE_DEFAULT = 0;

    double scale(const int noteID, const int scaleID = SCALE_DEFAULT) {
        switch (scaleID) {
        case SCALE_DEFAULT: default:
            return 256 * pow(1.0594630943592952645618252949463, noteID);
        }
    }

    struct envelope {
        virtual double getAmplitude(const double time, const double timeOn, const double timeOff) = 0;
    };

    // envelope for attack, decay, sustain, release - it's for key stroke
    struct envelopeADSR : public envelope {
        double attackTime;
        double decayTime;
        double releaseTime;

        double sustainAmplitude;
        double startAmplitude;

        envelopeADSR() {
            // 10 ms
            attackTime = 0.01;
            decayTime = 10.0;
            releaseTime = 1.0;

            startAmplitude = 1.0;
            sustainAmplitude = 0.2;
        }

        virtual double getAmplitude(const double time, const double timeOn, const double timeOff) {
            double amplitude = 0.0;
            double releaseAmplitude = 0.0;

            if (timeOn > timeOff) { // note is on
                double lifetimeOfEnvelope = time - timeOn;

                // attack
                // attack time period in if statement
                if (lifetimeOfEnvelope <= attackTime) {
                    // normalising the 0 -> 1 phase of amplitude by dividing lifetime by the attackTime
                    amplitude = (lifetimeOfEnvelope / attackTime) * startAmplitude;
                }

                // decay
                // decay time period in if statement
                if (lifetimeOfEnvelope > attackTime && lifetimeOfEnvelope <= (attackTime + decayTime)) {
                    amplitude = ((lifetimeOfEnvelope - attackTime) / decayTime) * (sustainAmplitude - startAmplitude) + startAmplitude;
                }

                // sustain
                // sustain time period in if statement
                if (lifetimeOfEnvelope > (attackTime + decayTime) /*&& lifetimeOfEnvelope <= (attackTime + decayTime + releaseTime)*/) {
                    amplitude = sustainAmplitude;
                }
            }
            else {  // note is off
                double lifetimeOfEnvelope = timeOff - timeOn;

                // if key was off before reaching sustain phase
                // attack
                if (lifetimeOfEnvelope <= attackTime) {
                    // normalising the 0 -> 1 phase of amplitude by dividing lifetime by the attackTime
                    releaseAmplitude = (lifetimeOfEnvelope / attackTime) * startAmplitude;
                }

                // decay
                if (lifetimeOfEnvelope > attackTime && lifetimeOfEnvelope <= (attackTime + decayTime)) {
                    releaseAmplitude = ((lifetimeOfEnvelope - attackTime) / decayTime) * (sustainAmplitude - startAmplitude) + startAmplitude;
                }

                // sustain
                if (lifetimeOfEnvelope > (attackTime + decayTime) /*&& lifetimeOfEnvelope <= (attackTime + decayTime + releaseTime)*/) {
                    releaseAmplitude = sustainAmplitude;
                }

                // release
                // normalising the 0 -> 1 phase of amplitude by dividing lifetime by the releaseTime
                amplitude = ((time - timeOff) / releaseTime) * (0.0 - releaseAmplitude) + releaseAmplitude;
            }

            // epsilon value check - to stop signals coming from envelope that we do not care of
            if (amplitude <= 0.0001) {
                amplitude = 0.0;
            }


            return amplitude;
        }
    };

    struct instrument {
        double volume;
        synth::envelopeADSR envelope;

        virtual double sound(const double time, synth::note note, bool &noteFinished) = 0;
    };

    struct harmonica : public instrument {
        harmonica() {
            envelope.attackTime = 0.05;
            envelope.decayTime = 1.0;
            envelope.releaseTime = 0.1;

            envelope.startAmplitude = 1.0;
            envelope.sustainAmplitude = 0.95;

            volume = 0.5;
        }

        virtual double sound(const double time, synth::note note, bool& noteFinished) {
            double amplitude = envelope.getAmplitude(time, note.timeOn, note.timeOff);
            if (amplitude <= 0.0) {
                noteFinished = true;
            }

            double sound =
                synth::oscillator(note.timeOn - time, synth::scale(note.id), synth::OSC_SQUARE, 5.0, 0.001)
                + 0.5 * synth::oscillator(note.timeOn - time, synth::scale(note.id + 12), synth::OSC_SQUARE)
                + 0.25 * synth::oscillator(note.timeOn - time, synth::scale(note.id + 24), synth::OSC_SQUARE)
                + 0.05 * synth::oscillator(note.timeOn - time, synth::scale(note.id + 24), OSC_NOISE);

            return amplitude * sound * volume;
        }
    };

    struct bell : public instrument {
        bell() {
            envelope.attackTime = 0.01;
            envelope.decayTime = 10.0;
            envelope.releaseTime = 1.0;

            envelope.startAmplitude = 1.0;
            envelope.sustainAmplitude = 0.2;

            volume = 0.5;
        }

        virtual double sound(const double time, synth::note note, bool& noteFinished) {
            double amplitude = envelope.getAmplitude(time, note.timeOn, note.timeOff);
            if (amplitude <= 0.0) {
                noteFinished = true;
            }
            
            double sound =
                synth::oscillator(note.timeOn - time, synth::scale(note.id + 12), synth::OSC_SINE, 5.0, 0.001)
                + 0.5 * synth::oscillator(note.timeOn - time, synth::scale(note.id + 24), synth::OSC_SINE)
                + 0.25 * synth::oscillator(note.timeOn - time, synth::scale(note.id + 36), synth::OSC_SINE);

            return amplitude * sound * volume;
        }
    };

    struct synthSound : public instrument {
        synthSound() {
            envelope.attackTime = 0.2;
            envelope.decayTime = 0.5;
            envelope.releaseTime = 0.5;

            envelope.startAmplitude = 1.0;
            envelope.sustainAmplitude = 0.8;

            volume = 0.1;
        }

        virtual double sound(const double time, synth::note note, bool& noteFinished) {
            double amplitude = envelope.getAmplitude(time, note.timeOn, note.timeOff);
            if (amplitude <= 0.0) {
                noteFinished = true;
            }

            double sound =
                0.5 * synth::oscillator(note.timeOn - time, synth::scale(note.id - 24), synth::OSC_SAW_ANALOGUE)
                + synth::oscillator(note.timeOn - time, synth::scale(note.id - 12), synth::OSC_SINE)
                + 0.25 * synth::oscillator(note.timeOn - time, synth::scale(note.id), synth::OSC_SQUARE)
                + 0.05 * synth::oscillator(note.timeOn - time, synth::scale(note.id + 12), OSC_NOISE);

            return amplitude * sound * volume;
        }
    };

    struct abba : public instrument {
        abba() {
            envelope.attackTime = 0.1;
            envelope.decayTime = 0.1;
            envelope.releaseTime = 1.0;

            envelope.startAmplitude = 1.0;
            envelope.sustainAmplitude = 0.7;

            volume = 0.2;
        }

        virtual double sound(const double time, synth::note note, bool& noteFinished) {
            double amplitude = envelope.getAmplitude(time, note.timeOn, note.timeOff);
            if (amplitude <= 0.0) {
                noteFinished = true;
            }

            double sound =
                1.0 * synth::oscillator(note.timeOn - time, synth::scale(note.id - 12), synth::OSC_SQUARE, 5.0, 0.005)
                + 0.5 * synth::oscillator(note.timeOn - time, synth::scale(note.id), synth::OSC_TRIANGLE, 5.0, 0.005)
                + 0.25 * synth::oscillator(note.timeOn - time, synth::scale(note.id + 12), synth::OSC_SAW_ANALOGUE, 5.0, 0.005);

            return amplitude * sound * volume;
        }
    };

    struct guitar : public instrument {
        guitar() {
            envelope.attackTime = 0.1;
            envelope.decayTime = 0.1;
            envelope.releaseTime = 1.0;

            envelope.startAmplitude = 1.0;
            envelope.sustainAmplitude = 0.7;

            volume = 0.2;
        }

        virtual double sound(const double time, synth::note note, bool& noteFinished) {
            double amplitude = envelope.getAmplitude(time, note.timeOn, note.timeOff);
            if (amplitude <= 0.0) {
                noteFinished = true;
            }

            double sound =
                /*0.25 * synth::oscillator(note.timeOn - time, synth::scale(note.id - 24), synth::OSC_SQUARE, 5.0, 0.005)*/
                + 0.75 * synth::oscillator(note.timeOn - time, synth::scale(note.id - 12), synth::OSC_TRIANGLE, 1.0, 0.005)
                + 0.1 * synth::oscillator(note.timeOn - time, synth::scale(note.id - 24), synth::OSC_SAW_ANALOGUE, 0.5, 0.005)
                + 1.0 * synth::oscillator(note.timeOn - time, synth::scale(note.id - 12), synth::OSC_SINE, 1.0, 0.01);

            return amplitude * sound * volume;
        }
    };
}

vector<synth::note> notes;
mutex muxNotes;

synth::bell bell;
synth::harmonica harmonica;
synth::synthSound synthSound;
synth::abba abba;
synth::guitar guitar;

        //    // for sweet dreams
        //    /*oscillator(frequencyOutput * 0.5, time, OSC_SAW_ANALOGUE)
        //    + oscillator(frequencyOutput, time, OSC_SINE)*/

typedef bool(*lambda)(synth::note const& item);
template<class T>
void safe_remove(T& v, lambda f)
{
    auto n = v.begin();
    while (n != v.end())
        if (!f(*n))
            n = v.erase(n);
        else
            ++n;
}

double MakeNoise(int channel, double time) {
    unique_lock<mutex> lm(muxNotes);
    double mixedOutput = 0.0;

    for (auto& note : notes) {
        bool noteFinished = false;
        double sound = 0.0;

        if (note.channel == 5) {
            sound = guitar.sound(time, note, noteFinished);
        }
        if (note.channel == 4) {
            sound = abba.sound(time, note, noteFinished);
        }
        if (note.channel == 3) {
            sound = synthSound.sound(time, note, noteFinished);
        }
        if (note.channel == 2) {
            sound = bell.sound(time, note, noteFinished);
        }
        if (note.channel == 1) {
            sound = harmonica.sound(time, note, noteFinished);
        }
        
        mixedOutput += sound;

        if (noteFinished && note.timeOff > note.timeOn) {
            note.isActive = false;
        }
    }

    safe_remove<vector<synth::note>>(notes, [](synth::note const& item) {return item.isActive; });

    return mixedOutput * 0.2;
}

int main()
{
    // get all sound hardware
    // short because we want 16 bit precision to the wave
    vector<wstring> devices = olcNoiseMaker<short>::Enumerate();

    // display found hardware
    /*for (auto device : devices) {
        wcout << "Found output device: " << device << endl;
    }*/
    //wcout << "Using device: " << devices[0] << endl;

    // create sound machine
    olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 512);

    // link MakeNoise with sound machine
    sound.SetUserFunction(MakeNoise);

    while (1) {
        // add keyboard
        //keyPressed = false;
        // making a piano keyboard made of normal keyboard
        for (int key = 0; key < 16; key++) {
            // 0x8000 is a code for clicked key
            short keyState = GetAsyncKeyState((unsigned char)("AWSEDFTGYHUJKOLP"[key]));

            double timeNow = sound.GetTime();

            muxNotes.lock();
            auto noteFound = find_if(notes.begin(), notes.end(), [&key](synth::note const& item) {return item.id == key; });

            if (noteFound == notes.end()) {
                // note not found in the vector
                if (keyState & 0x8000) {
                    synth::note note;
                    note.id = key;
                    note.timeOn = timeNow;
                    // HERE CHANGE THE INSTRUMENT
                    note.channel = 5;
                    note.isActive = true;

                    notes.emplace_back(note);

                    wcout << note.id << endl;
                }
                else {
                    // Note not in vector and key released, nothing to do
                }
            }
            else {
                if (keyState & 0x8000) {
                    if (noteFound->timeOff > noteFound->timeOn) {
                        // note is in vector and key pressed
                        noteFound->timeOn = timeNow;
                        noteFound->isActive = true;
                    }
                }
                else {
                    // key released
                    if (noteFound->timeOff < noteFound->timeOn) {
                        noteFound->timeOff = timeNow;
                    }
                }
            }

            muxNotes.unlock();
        }
        wcout << "\rNotes: " << notes.size() << "   ";
    }
       
    return 0;
}
