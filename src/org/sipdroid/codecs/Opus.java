/*
 * Copyright (C) 2015 The Lumicall Open Source Project
 * 
 * This file is part of Lumicall (http://lumicall.org)
 * 
 * Lumicall is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this source code; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
package org.sipdroid.codecs;

import org.sipdroid.sipua.ui.Sipdroid;

class Opus extends CodecBase implements Codec {   
	/* 
	 *                 | fs (Hz) | BR (kbps)
	 * ----------------+---------+---------
	 * Narrowband	   | 8000    | 6 -20
	 * Mediumband      | 12000   | 7 -25
	 * Wideband        | 16000   | 8 -30
	 * Super Wideband  | 24000   | 12 -40
	 *
	 * Table 1: fs specifies the audio sampling frequency in Hertz (Hz); BR
	 * specifies the adaptive bit rate range in kilobits per second (kbps).
	 * 
	 * Complexity can be scaled to optimize for CPU resources in real-time,
	 * mostly in trade-off to network bit rate. 0 is least CPU demanding and
	 * highest bit rate. 
	 */
	private static final int DEFAULT_COMPLEXITY = 0;
	
	Opus() {
		CODEC_USER_NAME = "opus";  
		CODEC_NAME = "opus";
		CODEC_DESCRIPTION = "6-20kbit"; 
		CODEC_NUMBER = 98;
		CODEC_DEFAULT_SETTING = "always";
                CODEC_SAMPLE_RATE = 8000;
                CODEC_FRAME_SIZE = 160;
		CODEC_JNI_LIB = "opus_jni";
		super.update();
	}
 
	public native int open(int compression);
	public native int decode(byte encoded[], short lin[], int size);
	public native int encode(short lin[], int offset, byte encoded[], int size);
	public native void close();

	public int open() {
		if(!isLoaded()) {
			throw new IllegalStateException("not loaded yet");
		}
		return open(DEFAULT_COMPLEXITY);
	}
}
