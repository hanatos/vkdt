use rawloader;
use libc::{c_void, c_char};
use std::ffi::CStr;
use std::str;

#[repr(C)]
pub struct c_rawimage {
  pub make        : [c_char;32],
  pub model       : [c_char;32],
  pub clean_make  : [c_char;32],
  pub clean_model : [c_char;32],
  pub width       : u64,
  pub height      : u64,
  pub cpp         : u64,
  pub wb_coeffs   : [f32;4],
  pub whitelevels : [u16;4],
  pub blacklevels : [u16;4],
  pub xyz_to_cam  : [[f32;3];4],
  pub filters     : u32,
  pub crop_aabb   : [u64;4],
  pub orientation : u32,
  // TODO: need data type indicator (u16 vs f32)
  // TODO: need stride because width may be truncated
  // TODO: need ox, oy so we can offset it when copying the buffer over to gpu
  pub data: *mut c_void,
}

#[no_mangle]
pub extern "C" fn rl_decode_file(
    filename: *mut c_char,
    rawimg  : *mut c_rawimage,
    ) -> u64
{
  let c_str: &CStr = unsafe { CStr::from_ptr(filename) };
  let strn : &str = c_str.to_str().unwrap();
  let mut image = rawloader::decode_file(strn).unwrap();
  let mut len = 0 as usize;
  unsafe{
  if let rawloader::RawImageData::Integer(ref mut vdat) = image.data
  {
    // XXX TODO: mark as u16 in the c struct too!
    len = vdat.len();
    (*rawimg).data = vdat.as_mut_ptr() as *mut c_void;
    std::mem::forget(image.data); // how do we clean this up??
  }
  else if let rawloader::RawImageData::Float(ref mut vdat) = image.data
  {
    len = vdat.len();
    (*rawimg).data = vdat.as_mut_ptr() as *mut c_void;
    std::mem::forget(image.data); // how do we clean this up??
  }

  (*rawimg).width  = image.width  as u64;
  (*rawimg).height = image.height as u64;
  (*rawimg).cpp    = image.cpp    as u64;
  for k in 0..4 { (*rawimg).wb_coeffs[k]   = image.wb_coeffs[k]; }
  for k in 0..4 { (*rawimg).whitelevels[k] = image.whitelevels[k]; }
  for k in 0..4 { (*rawimg).blacklevels[k] = image.blacklevels[k]; }
  for j in 0..3 { for i in 0..4 { (*rawimg).xyz_to_cam[i][j] = image.xyz_to_cam[i][j]; } }
  (*rawimg).orientation = image.orientation.to_u16() as u32; // ???

  // TODO: add 0x8827 ISO to Tag:: in tiff.rs and fetch it here to hand over

  // store aabb (x y X Y)
  (*rawimg).crop_aabb[0] = image.crops[3];
  (*rawimg).crop_aabb[1] = image.crops[0];
  (*rawimg).crop_aabb[2] = image.width  - image.crops[1];
  (*rawimg).crop_aabb[3] = image.height - image.crops[2];

  // XXX TODO: CFA dance: do this while we can still access CFA.color_at(row,col)
  // the alternative is to copy over the largish 48x48 pattern and port color_at in c.

  // XXX i think we need not these dances:
  // uncrop bayer sensor filter
  // mod->img_param.filters = mod_data->d->mRaw->cfa.getDcrawFilter();
  // if(mod->img_param.filters != 9u)
  //   mod->img_param.filters = rawspeed::ColorFilterArray::shiftDcrawFilter(
  //       mod_data->d->mRaw->cfa.getDcrawFilter(),
  //       cropTL.x, cropTL.y);

  // now we need to account for the pixel shift due to an offset filter:
  // dt_roi_t *ro = &mod->connector[0].roi;
  let ox = 0 as i32;
  let oy = 0 as i32;

  image.filters = 0x49494949; // something not 0 and not 9, indicating bayer
  // special handling for x-trans sensors
  if image.cfa.width == 6
  { // find first green in same row
    image.filters = 9;
    for i in 0..6 
    {
      if image.cfa.color_at(0, i) == 1
      {
        ox = i;
        break;
      }
    }
    if image.cfa.color_at(0, ox+1) != 1 && image.cfa.color_at(0, ox+2) != 1
    { // center of green x, need to go 2 down
      oy = 2;
      ox = (ox + 2) % 3;
    }
    if image.cfa.color_at(oy+1, ox) == 1
    { // two greens above one another, need to go down one
      oy++;
    }
    if image.cfa.color_at(oy, ox+1) == 1
    { // if x+1 is green, too, either x++ or x-=2 if x>=2
      if ox >= 2 { ox -= 2; }
      else { ox++; }
    }
    // now we should be at the beginning of a green 5-cross block.
    if image.cfa.color_at(oy, ox+1) == 2
    { // if x+1 == red and y+1 == blue, all good!
      // if not, x+=3 or y+=3, equivalently.
      if ox < oy  { ox += 3; }
      else        { oy += 3; }
    }
  }
  else // move to std bayer sensor offset
  {
    if image.cfa.color_at(0, 0) == 1
    {
      if image.cfa.color_at(0, 1) == 0 { ox = 1; }
      if image.cfa.color_at(0, 1) == 2 { oy = 1; }
    }
    else if image.cfa.color_at(0, 0) == 2
    {
      ox = oy = 1;
    }
  }
  // unfortunately need to round to full 6x6 block for xtrans
  let block = if image.cfa.width == 6 { 3 } else { 2 };
  let bigblock = image.cfa.width;
  // crop aabb is relative to buffer we emit,
  // so we need to move it along to the CFA pattern boundary
  let ref b = (*rawimg).crop_aabb;
  b[2] -= ox;
  b[3] -= oy;
  // and also we'll round it to cut only between CFA blocks
  b[0] = ((b[0] + bigblock - 1) / bigblock) * bigblock;
  b[1] = ((b[1] + bigblock - 1) / bigblock) * bigblock;
  b[2] =  (b[2] / block) * block;
  b[3] =  (b[3] / block) * block;

  // TODO: also store unprocessed stride and pass to c
  (*rawimg).width  -= ox;
  (*rawimg).height -= oy;
  // round down to full block size:
  (*rawimg).width  = ((*rawimg).width /block)*block;
  (*rawimg).height = ((*rawimg).height/block)*block;
  }
  len as u64
}

// // XXX TODO: and then get rid of the buffer again
// #[no_mangle]
// pub unsafe extern "C" fn deallocate_rust_buffer(ptr: *mut i32, len: ffi::size_t) {
//     let len = len as usize;
//     drop(Vec::from_raw_parts(ptr, len, len));
// }
