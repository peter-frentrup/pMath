#define _WIN32_WINNT  0x0600

#include "util.h"

#include <pmath.h>
#include <shlobj.h>


struct name_and_folderid_t {
  const char *name;
  const KNOWNFOLDERID *id;
};

#define  NAME_AND_FOLDERID( NAME )  { #NAME , &FOLDERID_ ## NAME }

static const struct name_and_folderid_t ordered_named_folder_ids[] = {
  NAME_AND_FOLDERID( AccountPictures ),
  NAME_AND_FOLDERID( AdminTools ),
  NAME_AND_FOLDERID( ApplicationShortcuts ),
  NAME_AND_FOLDERID( CameraRoll ),
  NAME_AND_FOLDERID( CDBurning ),
  NAME_AND_FOLDERID( CommonAdminTools ),
  NAME_AND_FOLDERID( CommonOEMLinks ),
  NAME_AND_FOLDERID( CommonPrograms ),
  NAME_AND_FOLDERID( CommonStartMenu ),
  NAME_AND_FOLDERID( CommonStartup ),
  NAME_AND_FOLDERID( CommonTemplates ),
  NAME_AND_FOLDERID( Contacts ),
  NAME_AND_FOLDERID( Cookies ),
  NAME_AND_FOLDERID( Desktop ),
  NAME_AND_FOLDERID( DeviceMetadataStore ),
  NAME_AND_FOLDERID( Documents ),
  NAME_AND_FOLDERID( DocumentsLibrary ),
  NAME_AND_FOLDERID( Downloads ),
  NAME_AND_FOLDERID( Favorites ),
  NAME_AND_FOLDERID( Fonts ),
  NAME_AND_FOLDERID( GameTasks ),
  NAME_AND_FOLDERID( History ),
  NAME_AND_FOLDERID( ImplicitAppShortcuts ),
  NAME_AND_FOLDERID( InternetCache ),
  NAME_AND_FOLDERID( Libraries ),
  NAME_AND_FOLDERID( Links ),
  NAME_AND_FOLDERID( LocalAppData ),
  NAME_AND_FOLDERID( LocalAppDataLow ),
  NAME_AND_FOLDERID( LocalizedResourcesDir ),
  NAME_AND_FOLDERID( Music ),
  NAME_AND_FOLDERID( MusicLibrary ),
  NAME_AND_FOLDERID( NetHood ),
  NAME_AND_FOLDERID( OriginalImages ),
  NAME_AND_FOLDERID( PhotoAlbums ),
  NAME_AND_FOLDERID( PicturesLibrary ),
  NAME_AND_FOLDERID( Pictures ),
  NAME_AND_FOLDERID( Playlists ),
  NAME_AND_FOLDERID( PrintHood ),
  NAME_AND_FOLDERID( Profile ),
  NAME_AND_FOLDERID( ProgramData ),
  NAME_AND_FOLDERID( ProgramFiles ),
  NAME_AND_FOLDERID( ProgramFilesX64 ),
  NAME_AND_FOLDERID( ProgramFilesX86 ),
  NAME_AND_FOLDERID( ProgramFilesCommon ),
  NAME_AND_FOLDERID( ProgramFilesCommonX64 ),
  NAME_AND_FOLDERID( ProgramFilesCommonX86 ),
  NAME_AND_FOLDERID( Programs ),
  NAME_AND_FOLDERID( Public ),
  NAME_AND_FOLDERID( PublicDesktop ),
  NAME_AND_FOLDERID( PublicDocuments ),
  NAME_AND_FOLDERID( PublicDownloads ),
  NAME_AND_FOLDERID( PublicGameTasks ),
  NAME_AND_FOLDERID( PublicLibraries ),
  NAME_AND_FOLDERID( PublicMusic ),
  NAME_AND_FOLDERID( PublicPictures ),
  NAME_AND_FOLDERID( PublicRingtones ),
  NAME_AND_FOLDERID( PublicUserTiles ),
  NAME_AND_FOLDERID( PublicVideos ),
  NAME_AND_FOLDERID( QuickLaunch ),
  NAME_AND_FOLDERID( Recent ),
//  NAME_AND_FOLDERID( RecordedTV ),
  NAME_AND_FOLDERID( RecordedTVLibrary ),
  NAME_AND_FOLDERID( ResourceDir ),
  NAME_AND_FOLDERID( Ringtones ),
  NAME_AND_FOLDERID( RoamingAppData ),
  NAME_AND_FOLDERID( RoamedTileImages ),
  NAME_AND_FOLDERID( RoamingTiles ),
  NAME_AND_FOLDERID( SampleMusic ),
  NAME_AND_FOLDERID( SamplePictures ),
  NAME_AND_FOLDERID( SamplePlaylists ),
  NAME_AND_FOLDERID( SampleVideos ),
  NAME_AND_FOLDERID( SavedGames ),
  NAME_AND_FOLDERID( SavedPictures ),
  NAME_AND_FOLDERID( SavedPicturesLibrary ),
  NAME_AND_FOLDERID( SavedSearches ),
  NAME_AND_FOLDERID( Screenshots ),
  NAME_AND_FOLDERID( SearchHistory ),
  NAME_AND_FOLDERID( SearchTemplates ),
  NAME_AND_FOLDERID( SendTo ),
  NAME_AND_FOLDERID( SidebarDefaultParts ),
  NAME_AND_FOLDERID( SidebarParts ),
  NAME_AND_FOLDERID( SkyDrive ),
  NAME_AND_FOLDERID( SkyDriveCameraRoll ),
  NAME_AND_FOLDERID( SkyDriveDocuments ),
  NAME_AND_FOLDERID( SkyDrivePictures ),
  NAME_AND_FOLDERID( StartMenu ),
  NAME_AND_FOLDERID( Startup ),
  NAME_AND_FOLDERID( System ),
  NAME_AND_FOLDERID( SystemX86 ),
  NAME_AND_FOLDERID( Templates ),
  NAME_AND_FOLDERID( UserPinned ),
  NAME_AND_FOLDERID( UserProfiles ),
  NAME_AND_FOLDERID( UserProgramFiles ),
  NAME_AND_FOLDERID( UserProgramFilesCommon ),
  NAME_AND_FOLDERID( Videos ),
  NAME_AND_FOLDERID( VideosLibrary ),
  NAME_AND_FOLDERID( Windows )
};

struct string_data_t {
  int             length;
  const uint16_t *buffer;
};

static int compare_known_folder_id(const void *_key, const void *_datum) {
  const struct string_data_t    *key   = _key;
  const struct name_and_folderid_t *datum = _datum;
  int len = key->length;
  const uint16_t *key_buffer = key->buffer;
  const uint8_t *datum_name = datum->name;
  
  for(; *datum_name && len > 0; --len, ++key_buffer, ++datum_name) {
    if(*key_buffer < *datum_name)
      return -1;
    if(*key_buffer > *datum_name)
      return 1;
  }
  
  if(*datum_name)
    return -1;
  if(len > 0)
    return 1;
  return 0;
}

static const KNOWNFOLDERID *get_known_folder_id(pmath_string_t name) {
  struct string_data_t key;
  struct name_and_folderid_t *result;
  
  key.length = pmath_string_length(name);
  key.buffer = pmath_string_buffer(&name);
  
  result = bsearch(
             &key,
             ordered_named_folder_ids,
             sizeof(ordered_named_folder_ids) / sizeof(ordered_named_folder_ids[0]),
             sizeof(ordered_named_folder_ids[0]),
             compare_known_folder_id);
  
  if(!result)
    return NULL;
  return result->id;
}

/** Convert a pMath object to a boolean and destroy it.
    \param name The option name. It won't be freed.
    \param more Options returned by pmath_options_extract(). Won't be freed.
    \param error Pointer to n error indicator. Set to TRUE on error. If it was set before, no message will be generated. 
 */
static
pmath_bool_t get_option_boolean(pmath_symbol_t name, pmath_t more, pmath_bool_t *error) {
  pmath_string_t name = 
  pmath_t value = pmath_option_value(PMATH_NULL, name, more);
  
  if(pmath_same(value, PMATH_SYMBOL_TRUE)) {
    pmath_unref(value);
    return TRUE;
  }
  if(pmath_same(value, PMATH_SYMBOL_FALSE)) {
    pmath_unref(value);
    return FALSE;
  }
  if(*error) {
    pmath_unref(value);
    return FALSE;
  }
  pmath_message(PMATH_NULL, "opttf", 2, name_obj, value);
  *error = TRUE;
  return FALSE;
}

pmath_t windows_GetAllKnownFolders(pmath_expr_t expr) {
  const struct name_and_folderid_t *current = ordered_named_folder_ids;
  const struct name_and_folderid_t *end = current + sizeof(ordered_named_folder_ids) / sizeof(ordered_named_folder_ids[0]);
  
  if(pmath_expr_length(expr) != 0) {
    pmath_message_argxxx(pmath_expr_length(expr), 0, 0);
    return expr;
  }
  pmath_unref(expr);
  
  pmath_gather_begin(PMATH_NULL);
  for(;current != end; ++current) {
    pmath_emit(PMATH_C_STRING(current->name), PMATH_NULL);
  }
  return pmath_gather_end();
}

pmath_t windows_SHGetKnownFolderPath(pmath_expr_t expr) {
  /* SHGetKnownFolderPath(knownFolderId)
     
     options:
      CreateDirectory -> False
     
     messages:
      SHGetKnownFolderPath::unk   Unrecognized folder id `1`.
      SHGetKnownFolderPath::hr    Windows HRESULT error `1` received.
   */
  pmath_string_t name;
  pmath_expr_t options;
  pmath_bool_t options_error = FALSE;
  const KNOWNFOLDERID *id;
  DWORD flags = KF_FLAG_DEFAULT;
  wchar_t *result;
  HRESULT hr;
  
  if(pmath_expr_length(expr) < 1) {
    pmath_message_argxxx(pmath_expr_length(expr), 1, 1);
    return expr;
  }
  
  name = pmath_expr_get_item(expr, 1);
  if(!pmath_is_string(name)) {
    pmath_unref(name);
    pmath_message(PMATH_NULL, "str", 2, PMATH_FROM_INT32(1), pmath_ref(expr));
    return expr;
  }
  
  id = get_known_folder_id(name);
  if(!id) {
    pmath_message(PMATH_NULL, "unk", 1, name);
    return expr;
  }
  pmath_unref(name);
  
  options = pmath_options_extract(expr, 1);
  if(pmath_is_null(options)) 
    return expr;
  
  if(get_option_boolean(PMATH_SYMBOL_CREATEDIRECTORY, options, &options_error))
    flags |= KF_FLAG_CREATE;
  pmath_unref(options);
  
  pmath_unref(expr);
  result = NULL;
  if(!check_succeeded(SHGetKnownFolderPath(id, flags, NULL, &result)))
    return pmath_ref(PMATH_SYMBOL_FAILED);
  
  if(!result)
    return pmath_ref(PMATH_SYMBOL_FAILED);
    
  name = pmath_string_insert_ucs2(PMATH_NULL, 0, result, -1);
  CoTaskMemFree(result);
  return name;
}
