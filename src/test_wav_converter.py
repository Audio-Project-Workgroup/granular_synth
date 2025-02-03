# tested on python 3.12

import numpy as np
import librosa
import soundfile as sf


sound_name = 'fingertips'
load_name = f'../data/{sound_name}.wav'

raw_audio, sr_original = librosa.load(load_name, sr=None, mono=False)

sample_rates = [44100, 48000, 96000]
bit_depths = [8, 16, 24, 32, 'float32']

for sample_rate in sample_rates:
    resampled_audio = librosa.resample(raw_audio, orig_sr=sr_original, target_sr=sample_rate, res_type='fft')
    for bit_depth in bit_depths:        
        if bit_depth == 8:
            reformatted_audio = np.clip((resampled_audio + 1)*127.5, 0, 255).astype(np.int16)
            subtype = 'PCM_U8'
        elif bit_depth == 16:
            reformatted_audio = np.clip(resampled_audio*32767, -32768, 32767).astype(np.int16)
            subtype = 'PCM_16'
        elif bit_depth == 24:
            reformatted_audio = np.clip(resampled_audio*8388607, -8388608, 8388608).astype(np.int32)
            subtype = 'PCM_24'
        elif bit_depth == 32:
            reformatted_audio = np.clip(resampled_audio*2147483647, -2147483648, 2147483647).astype(np.int32)
            subtype = 'PCM_32'
        elif bit_depth == 'float32':
            reformatted_audio = resampled_audio.astype(np.float32)
            subtype = 'FLOAT'

        if sf.check_format('WAV', subtype):
            write_name = f"../data/{sound_name}_{sample_rate}_{subtype}.wav"            
            print(f"writing file {write_name}")
            sf.write(write_name, reformatted_audio.T, sample_rate, subtype=subtype)
        else:
            print(f"invalid format: sampleRate = {sample_rate}, subtype={subtype}")
