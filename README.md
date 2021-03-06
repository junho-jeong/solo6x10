Bluecherry solo6x10 Video4Linux2 driver
======================================
This driver is released under the GPL for the line of Bluecherry MPEG-4 and H.264 capture cards (PCI, PCIe, Mini-PCI).
www.bluecherrydvr.com [1]

Requirements
------------
- A Bluecherry BC-* capture card
- A newer Linux kernel > 2.6.32

Compiling
---------
Simply download and run sudo make ; sudo make install ; sudo depmod -a  

Public bug / feature tracker
---------------------------
The public bug and feature tracker for the solo6x10 driver can be found here:
http://improve.bluecherrydvr.com/projects/solo6010-v2 [2]

General questions, comments, suggestions
----------------------------------------
Please visit our support page: http://support.bluecherrydvr.com/forums/214485-hardware-compression-mpeg-4-h-264-driver [3]

FAQs
====

So, I installed the driver, how do I verify my card is detected?
---------------------------------------------------------------
The card should be detected with lspci.  An example would look like this:

04:05.0 Multimedia video controller: Bluecherry BC-H16480A 16 port H.264 video and audio encoder / decoder

	dmesg |grep -i solo 

will also show this:

	[   12.822837] solo6x10 0000:04:05.0: Probing Softlogic 6110
	[   12.823108] solo6x10 0000:04:05.0: PCI INT A -> Link[APC6] -> GSI 16 (level, low) -> IRQ 16
	[   12.824294] solo6x10 0000:04:05.0: Enabled 2 i2c adapters
	[   12.824332] solo6x10 0000:04:05.0: Using NTSC video format
	[   15.760706] solo6x10 0000:04:05.0: Initialized 4 tw28xx chips: tw2864[4]
	[   15.760783] solo6x10 0000:04:05.0: Display as /dev/video0 with 16 inputs (5 extended)
	[   15.788965] solo6x10 0000:04:05.0: Encoders as /dev/video1-16
	[   15.789372] solo6x10 0000:04:05.0: Alsa sound card as Softlogic0

What is the display port?
-------------------------
The display port is what is going to be displayed out of the composite video output on the back of the card.  
Basically think of this as a built-in quad or multiplexer.  You can display one camera, or several different 'layouts' 
on this display port.

The display port allows you to view and configure what is shown on the video out port of the card. The device has several inputs 
and depends on which card you have installed.

4-port: 1 input per port and 1 virtual input for all 4 inputs in 4-up mode.
8-port: 1 input per port and 2 virtual inputs for 4-up on inputs 1-4 and 5-8 respectively.
16-port: 1 input per port and 5 virtual inputs for 4-up on inputs 1-5, 5-8, 9-12 and 13-16 and 1 virtual input for 16-up on all inputs.

You do not have to open this device for the video output of the card to work. If you wanted to display channels 1-4 in 'quad view' on the 
display port you could simply change the inputs with V4L2 ioctl's or with mplayer:

	mplayer tv://0/4 

Great, my card is detected, how do I pull video from it?
--------------------------------------------------------
You can use mplayer to get a feed from the MJPEG encoder:

	mplayer -tv device=/dev/video1:outfmt=mjpeg tv://

Can I use any Video4Linux2 application to pull encoded MPEG-4 or H.264 video?
-----------------------------------------------------------------------------
Yes and no.  

Unlike any card before this supported by v4l2, our hardware compression cards produce containerless MPEG-4 frames. 
Most v4l2 applications expect some sort of MPEG-2 stream such as program or transport. 
Since these programs do not expect MPEG-4 raw frames, We are not aware of any that are capable of playing the 
encoders directly (much less being able to record from them). You can do something simple like 'cat /dev/video1' 
and pipe it to vlc, or write a program that just writes the frames to disk (most programs can play the raw m4v files produced from the driver).

Below is a mplayer example on how to play the first channels MJPEG feed:

mplayer -tv device=/dev/video1:outfmt=mjpeg tv://

How does the audio work?
------------------------
The cards produce what is known as G.723, which is a voice codec typically found on phone systems (especially VoIP).

Since Alsa currently doesn't have a format for G.723, the driver shows it as unsigned 8-bit PCM audio.  We have sent a patch that was included 
in alsa-kernel (hopefully getting synced to mainline soon). But this only defines the correct format, it doesn't change the way you handle it at all.  
You must convert G.723-24 (3-bit samples at 8khz) yourself. 

I'm a developer and I want to add support for the Bluecherry H.264 cards in my software, can I get a demo?
----------------------------------------------------------------------------------------------------------
We are very interested in speaking with core developers of popular Video4Linux2 applications to add support for these cards.  
Please contact support@bluecherry.net for information

[1]: http://www.bluecherrydvr.com
[2]: http://improve.bluecherrydvr.com/projects/solo6010-v2
[3]: http://support.bluecherrydvr.com/forums/214485-hardware-compression-mpeg-4-h-264-driver

