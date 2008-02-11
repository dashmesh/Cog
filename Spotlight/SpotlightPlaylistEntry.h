//
//  SpotlightPlaylistEntry.h
//  Cog
//
//  Created by Matthew Grinshpun on 11/02/08.
//  Copyright 2008 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "PlaylistEntry.h"


@interface SpotlightPlaylistEntry : PlaylistEntry

+(SpotlightPlaylistEntry *)playlistEntryWithMetadataItem:(NSMetadataItem *)metadataItem;

@end
