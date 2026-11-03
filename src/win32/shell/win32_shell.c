// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: @per_os_impl Shell Operations

internal void
sh_message(B32 error, String8 title, String8 message)
{
  Temp scratch = scratch_begin(0, 0);
  String16 title16 = str16_from_8(scratch.arena, title);
  String16 message16 = str16_from_8(scratch.arena, message);
  MessageBoxW(0, (WCHAR *)message16.str, (WCHAR *)title16.str, MB_OK|(!!error*MB_ICONERROR));
  scratch_end(scratch);
}

internal String8
sh_pick_file(Arena *arena, String8 title, String8 initial_path)
{
  String8 result = {0};
  {
    Temp scratch = scratch_begin(&arena, 1);
    U64 buffer_size = 4096;
    U16 *buffer = push_array(scratch.arena, U16, buffer_size);
    OPENFILENAMEW params = {sizeof(params)};
    {
      params.lpstrTitle = (WCHAR *)str16_from_8(scratch.arena, title).str;
      params.lpstrFile = (WCHAR *)buffer;
      params.nMaxFile = buffer_size;
      params.lpstrInitialDir = (WCHAR *)str16_from_8(scratch.arena, initial_path).str;
    }
    if(GetOpenFileNameW(&params))
    {
      result = str8_from_16(arena, str16_cstring((U16 *)buffer));
    }
    scratch_end(scratch);
  }
  return result;
}

internal String8
sh_pick_folder(Arena *arena, String8 title, String8 initial_path)
{
  // NOTE(rad): GetOpenFileNameW cannot pick folders; the common item dialog
  // (IFileOpenDialog + FOS_PICKFOLDERS) can. COM is initialized here too, so
  // this also works from console builds (wWinMain is the only other place).
  String8 result = {0};
  Temp scratch = scratch_begin(&arena, 1);
  HRESULT init_result = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
  IFileOpenDialog *dialog = 0;
  if(SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog, 0, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&dialog)))
  {
    //- rad: options & title
    FILEOPENDIALOGOPTIONS options = 0;
    dialog->lpVtbl->GetOptions(dialog, &options);
    dialog->lpVtbl->SetOptions(dialog, options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
    dialog->lpVtbl->SetTitle(dialog, (WCHAR *)str16_from_8(scratch.arena, title).str);

    //- rad: initial folder
    if(initial_path.size != 0)
    {
      String8 initial_path_copy = push_str8_copy(scratch.arena, initial_path);
      for(U64 idx = 0; idx < initial_path_copy.size; idx += 1)
      {
        if(initial_path_copy.str[idx] == '/')
        {
          initial_path_copy.str[idx] = '\\';
        }
      }
      IShellItem *initial_item = 0;
      if(SUCCEEDED(SHCreateItemFromParsingName((WCHAR *)str16_from_8(scratch.arena, initial_path_copy).str, 0, &IID_IShellItem, (void **)&initial_item)))
      {
        dialog->lpVtbl->SetFolder(dialog, initial_item);
        initial_item->lpVtbl->Release(initial_item);
      }
    }

    //- rad: show (modal), read the picked folder's file system path
    if(SUCCEEDED(dialog->lpVtbl->Show(dialog, 0)))
    {
      IShellItem *item = 0;
      if(SUCCEEDED(dialog->lpVtbl->GetResult(dialog, &item)))
      {
        WCHAR *path16 = 0;
        if(SUCCEEDED(item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &path16)))
        {
          result = str8_from_16(arena, str16_cstring((U16 *)path16));
          CoTaskMemFree(path16);
        }
        item->lpVtbl->Release(item);
      }
    }
    dialog->lpVtbl->Release(dialog);
  }
  if(SUCCEEDED(init_result))
  {
    CoUninitialize();
  }
  scratch_end(scratch);
  return result;
}

internal void
sh_show_in_file_browser(String8 path)
{
  Temp scratch = scratch_begin(0, 0);
  String8 path_copy = push_str8_copy(scratch.arena, path);
  for(U64 idx = 0; idx < path_copy.size; idx += 1)
  {
    if(path_copy.str[idx] == '/')
    {
      path_copy.str[idx] = '\\';
    }
  }
  String16 path16 = str16_from_8(scratch.arena, path_copy);
  SFGAOF flags = 0;
  PIDLIST_ABSOLUTE list = 0;
  if(path16.size != 0 && SUCCEEDED(SHParseDisplayName(path16.str, 0, &list, 0, &flags)))
  {
    HRESULT hr = SHOpenFolderAndSelectItems(list, 0, 0, 0);
    CoTaskMemFree(list);
    (void)hr;
  }
  scratch_end(scratch);
}

internal void
sh_open_in_browser(String8 url)
{
  Temp scratch = scratch_begin(0, 0);
  String16 url16 = str16_from_8(scratch.arena, url);
  ShellExecuteW(0, L"open", (WCHAR *)url16.str, 0, 0, SW_SHOWNORMAL);
  scratch_end(scratch);
}

internal B32
sh_install_or_uninstall_self(B32 write, B32 install)
{
  // TODO(rjf)
  return 0;
}
