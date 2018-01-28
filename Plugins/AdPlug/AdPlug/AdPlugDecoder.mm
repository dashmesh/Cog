//
//  AdPlugDecoder.m
//  AdPlug
//
//  Created by Christopher Snowhill on 1/27/18.
//  Copyright 2018 __LoSnoCo__. All rights reserved.
//

#import "AdPlugDecoder.h"

#import <AdPlug/nemuopl.h>

#import "fileprovider.h"

#import "Logging.h"

#import "PlaylistController.h"

@implementation AdPlugDecoder

- (id)init
{
    self = [super init];
    if (self) {
        m_player = NULL;
        m_emu = NULL;
    }
    return self;
}

- (BOOL)open:(id<CogSource>)s
{
	[self setSource:s];

    m_emu = new CNemuopl(44100);

    NSString * path = [[s url] absoluteString];
    NSRange fragmentRange = [path rangeOfString:@"#" options:NSBackwardsSearch];
    if (fragmentRange.location != NSNotFound) {
        path = [path substringToIndex:fragmentRange.location];
    }
    
    std::string _path = [[path stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding] UTF8String];
    m_player = CAdPlug::factory(_path, m_emu, CAdPlug::players, CProvider_cog( _path, source ));
    
    if ( !m_player )
        return 0;
    
    if ([[source.url fragment] length] == 0)
        subsong = 0;
    else
        subsong = [[source.url fragment] intValue];
    
    samples_todo = 0;

    length = m_player->songlength(subsong) * 441 / 10;
    current_pos = 0;
    
    m_player->rewind(subsong);

    [self willChangeValueForKey:@"properties"];
	[self didChangeValueForKey:@"properties"];

	return YES;
}

- (NSDictionary *)properties
{
	return [NSDictionary dictionaryWithObjectsAndKeys:
		[NSNumber numberWithInt:0], @"bitrate",
		[NSNumber numberWithFloat:44100], @"sampleRate",
		[NSNumber numberWithDouble:length], @"totalFrames",
		[NSNumber numberWithInt:16], @"bitsPerSample", //Samples are short
        [NSNumber numberWithBool:NO], @"floatingPoint",
		[NSNumber numberWithInt:2], @"channels", //output from gme_play is in stereo
		[NSNumber numberWithBool:YES], @"seekable",
		@"host", @"endian",
		nil];
}

- (int)readAudio:(void *)buf frames:(UInt32)frames
{
    int total = 0;
    bool dont_loop = !IsRepeatOneSet();
    if ( dont_loop && current_pos + frames > length )
        frames = (UInt32) (length - current_pos);
    while ( total < frames )
    {
        bool running = true;
        if ( !samples_todo )
        {
            running = m_player->update() && running;
            if ( !dont_loop || running )
            {
                samples_todo = 44100 / m_player->getrefresh();
                current_pos += samples_todo;
            }
        }
        if ( !samples_todo )
            break;
        int samples_now = samples_todo;
        if ( samples_now > (frames - total) )
            samples_now = frames - total;
        m_emu->update( (short*)buf, samples_now );
        buf = ((short *)buf) + samples_now * 2;
        samples_todo -= samples_now;
        total += samples_now;
    }
    
    return total;
}

- (long)seek:(long)frame
{
    if ( frame < current_pos )
    {
        current_pos = 0;
        m_player->rewind( subsong );
    }
    
    while ( current_pos < frame )
    {
        m_player->update();
        current_pos += 44100 / m_player->getrefresh();
    }
    
    samples_todo = (UInt32) (frame - current_pos);
   
    return frame;
}

- (void)cleanUp
{
    delete m_player; m_player = NULL;
    delete m_emu; m_emu = NULL;
}

- (void)close
{
	[self cleanUp];
	
	if (source) {
		[source close];
		[self setSource:nil];
	}
}

- (void)dealloc
{
    [self close];
}

- (void)setSource:(id<CogSource>)s
{
	source = s;
}

- (id<CogSource>)source
{
	return source;
}

+ (NSArray *)fileTypes 
{
    const CPlayers & pl = CAdPlug::players;
    CPlayers::const_iterator i;
    unsigned j;
    NSMutableArray * array = [NSMutableArray array];
    
    for (i = pl.begin(); i != pl.end(); ++i) {
        for ( j = 0; ( *i )->get_extension(j); ++j )
        {
            [array addObject:[NSString stringWithUTF8String:(*i)->get_extension(j) + 1]];
        }
    }
    
    return array;
}

+ (NSArray *)mimeTypes 
{	
	return nil;
}

+ (float)priority
{
    return 0.5;
}

@end
