# .gdbinit file for debugging Mozilla

# Don't stop for the SIG32/33/etc signals that Flash produces
handle SIG32 noprint nostop pass
handle SIG33 noprint nostop pass
handle SIGPIPE noprint nostop pass

# Show the concrete types behind nsIFoo
set print object on

# run when using the auto-solib-add trick
def prun
        tbreak main
        run
        set auto-solib-add 0
        cont
end

# run -mail, when using the auto-solib-add trick
def pmail
        tbreak main
        run -mail
        set auto-solib-add 0
        cont
end

# Define a "pu" command to display PRUnichar * strings (100 chars max)
# Also allows an optional argument for how many chars to print as long as
# it's less than 100.
def pu
  set $uni = $arg0
  if $argc == 2
    set $limit = $arg1
    if $limit > 100
      set $limit = 100
    end
  else
    set $limit = 100
  end
  # scratch array with space for 100 chars plus null terminator.  Make
  # sure to not use ' ' as the char so this copy/pastes well.
  set $scratch = "____________________________________________________________________________________________________"
  set $i = 0
  set $scratch_idx = 0
  while (*$uni && $i++ < $limit)
    if (*$uni < 0x80)
      set $scratch[$scratch_idx++] = *(char*)$uni++
    else
      if ($scratch_idx > 0)
        set $scratch[$scratch_idx] = '\0'
        print $scratch
        set $scratch_idx = 0
      end
      print /x *(short*)$uni++
    end
  end
  if ($scratch_idx > 0)
    set $scratch[$scratch_idx] = '\0'
    print $scratch
  end
end

# Define a "ps" command to display subclasses of nsAC?String.  Note that
# this assumes strings as of Gecko 1.9 (well, and probably a few
# releases before that as well); going back far enough will get you
# to string classes that this function doesn't work for.
def ps
  set $str = $arg0
  if (sizeof(*$str.mData) == 1 && ($str.mFlags & 1) != 0)
    print $str.mData
  else
    pu $str.mData $str.mLength
  end
end

# Define a "pa" command to display the string value for an nsIAtom
def pa
  set $atom = $arg0
  if (sizeof(*((&*$atom)->mString)) == 2)
    pu (&*$atom)->mString
  end
end

# define a "pxul" command to display the type of a XUL element from
# an nsXULDocument* pointer.
def pxul
  set $p = $arg0
  print $p->mNodeInfo.mRawPtr->mInner.mName->mStaticAtom->mString
end

# define a "prefcnt" command to display the refcount of an XPCOM obj
def prefcnt
  set $p = $arg0
  print ((nsPurpleBufferEntry*)$p->mRefCnt.mTagged)->mRefCnt
end

# define a "ptag" command to display the tag name of a content node
def ptag
  set $p = $arg0
  pa $p->mNodeInfo.mRawPtr->mInner.mName
end

##
## nsTArray
##
define ptarray
        if $argc == 0
                help ptarray
        else
                set $size = $arg0.mHdr->mLength
                set $capacity = $arg0.mHdr->mCapacity
                set $size_max = $size - 1
                set $elts = $arg0.Elements()
        end
        if $argc == 1
                set $i = 0
                while $i < $size
                        printf "elem[%u]: ", $i
                        p *($elts + $i)
                        set $i++
                end
        end
        if $argc == 2
                set $idx = $arg1
                if $idx < 0 || $idx > $size_max
                        printf "idx1, idx2 are not in acceptable range: [0..%u].\n", $size_max
                else
                        printf "elem[%u]: ", $idx
                        p *($elts + $idx)
                end
        end
        if $argc == 3
          set $start_idx = $arg1
          set $stop_idx = $arg2
          if $start_idx > $stop_idx
            set $tmp_idx = $start_idx
            set $start_idx = $stop_idx
            set $stop_idx = $tmp_idx
          end
          if $start_idx < 0 || $stop_idx < 0 || $start_idx > $size_max || $stop_idx > $size_max
            printf "idx1, idx2 are not in acceptable range: [0..%u].\n", $size_max
          else
            set $i = $start_idx
                while $i <= $stop_idx
                        printf "elem[%u]: ", $i
                        p *($elts + $i)
                        set $i++
                end
          end
        end
        if $argc > 0
                printf "nsTArray length = %u\n", $size
                printf "nsTArray capacity = %u\n", $capacity
                printf "Element "
                whatis *$elts
        end
end

document ptarray
        Prints nsTArray information.
        Syntax: ptarray   
        Note: idx, idx1 and idx2 must be in acceptable range [0...size()-1].
        Examples:
        ptarray a - Prints tarray content, size, capacity and T typedef
        ptarray a 0 - Prints element[idx] from tarray
        ptarray a 1 2 - Prints elements in range [idx1..idx2] from tarray
end

def js
  call DumpJSStack()
end

def ft
  call nsFrame::DumpFrameTree($arg0)
end

#####################################################################################
#
# MIPS Additions:
#    gdb currently needs Mozilla code to appears to be under Android mr1 repo via symlink.
#


def set_ElfLoader_breakpoints
  break breakpoint
  break __wrap_dlopen
  break __wrap_dlerror
  break __wrap_dlsym
  break __wrap_dlclose
  break __wrap_dladdr
  break __wrap_dl_iterate_phdr
  break SystemElf::GetSymbolPtr
  break SystemElf::Load
  break ElfLoader::Load
end

document set_ElfLoader_breakpoints
        Sets breakpoints in mozglue/linker/ElfLoader.cpp
end

def set_CustomElf_brealpoints
  break breakpoint
  break CustomElf::Load
  break CustomElf::LoadSegment
  break CustomElf::InitDyn
  break CustomElf::Relocate
  break CustomElf::RelocateJumps
  break CustomElf::RelocateGotEntries
  break CustomElf::CallInit
  break CustomElf::CallFini
end

document set_CustomElf_brealpoints
        Sets CustomElf breakpoints for debugging CustomElf MIPS Code.
end

def set_dvm_breakpoints
  break breakpoint
  break setup_nss_functions
  break dvmCallJNIMethod
  break dvmPlatformInvoke
  break dvmResolveNativeMethod:115
  break dvmCheckCallJNIMethod
  break dvmCallJNIMethod
end

document set_dvm_breakpoints
        Sets dvm breakpoints for debugging Linking of MIPS Code.
end

def set_apk_breakpoints
  break loadGeckoLibs
  break extractFile
  break extractLib
  break getLibraryCache
  break ensureLibCache
  break fillLibCache
  break lookupLibCacheFd
  break addLibCacheFd
  break mozload
end

document set_apk_breakpoints
        Sets APKOpen breakpoints for debugging Linking of MIPS Code. Code not currently compiled in.
end

def set_other_linker_breakpoints
  break insert_soinfo_into_debug_map
  break remove_soinfo_from_debug_map
  break notify_gdb_of_load
  break notify_gdb_of_unload
  break notify_gdb_of_libraries
  break alloc_info
  break free_info
# break dl_unwind_find_exidx
  break dl_iterate_phdr
  break _elf_lookup
# break elfhash
  break _do_lookup
  break lookup_in_library
  break lookup
  break find_containing_library
  break find_containing_symbol
  break _open_lib
  break open_library

# break is_prelinked
# break verify_elf_object
# break get_lib_extents
# break reserve_mem_region
# break alloc_mem_region
# break load_segments
# break load_library
# break load_mapped_library
# break init_library
  break find_library
# break find_mapped_library

  break unload_library
  break reloc_library
end

document set_other_linker_breakpoints
        Sets other-licenses/android/linker.c  breakpoints for debugging Linking of MIPS Code.
end

def set_solib_stuff
#  set solib-absolute-prefix /home/piet/src/mr1/out/target/product/generic_mips/symbols
#  set solib-search-path     /home/piet/src/mr1/out/target/product/generic_mips/symbols/system/lib:/home/piet/src/mr1/mozilla/src.3/objdir-droid/mozglue/build:/home/piet/src/mozilla/src.3/objdir-droid/mozglue/build

  set solib-absolute-prefix /home/piet/src/mr1/mozilla/src.3/objdir-droid/mozglue/build
  set solib-search-path     /home/piet/src/mr1/mozilla/src.3/objdir-droid/mozglue/build:/home/piet/src/mr1/out/target/product/generic_mips/symbols/system/lib
end

document set_solib_stuff
        Set solib-absolute-prefix and solib-search-path so source code is found.
end

def setup
  set_solib_stuff
  set_ElfLoader_breakpoints
# set_CustomElf_brealpoints
# set_dvm_breakpoints
end

document setup
   set shared library paths and initial breakpoints
end


