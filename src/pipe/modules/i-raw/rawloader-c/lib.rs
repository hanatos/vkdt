use rawler;
use libc::{c_void, c_char};
use std::ffi::CStr;
use std::str;
use std::cmp;
use std::fs::File;
use std::io::BufReader;
use std::convert::TryInto;
use rawler::rawimage::RawPhotometricInterpretation;
use rawler::Result;
use rawler::imgop::xyz::Illuminant;
use rawler::decoders::RawDecodeParams;

#[repr(C)]
pub struct c_rawimage {
  pub maker       : [c_char;32],
  pub model       : [c_char;32],
  pub clean_maker : [c_char;32],
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

  pub iso         : f32,
  pub exposure    : f32,
  pub aperture    : f32,
  pub focal_length: f32,
  pub datetime    : [c_char;32],

  pub data_type   : u32,   // 0 means u16, 1 means f32
  pub cfa_off_x   : u32,
  pub cfa_off_y   : u32,
  pub stride      : u32,
  pub data: *mut c_void,
}

fn copy_string(
    src: &String,
    dst: &mut [c_char;32])
{
  for (i, c) in src.chars().enumerate() {
    if i > 30 { break; }
    dst[i] = c as i8;
  }
  dst[cmp::min(src.len(),31)] = 0;
}

unsafe fn copy_metadata(path : &str, rawimg : *mut c_rawimage) -> Result<()>
{
  let input = BufReader::new(File::open(&path).map_err(|e| rawler::RawlerError::with_io_error("load into buffer", &path, e))?);
  let mut rawfile = rawler::RawFile::new(&path, input);
  let decoder = rawler::get_decoder(&mut rawfile)?;
  let md = decoder.raw_metadata(&mut rawfile, RawDecodeParams::default())?;
  // let iso = u32::from(md.exif.iso_speed.unwrap()); // is already u32 and should be preferred?
  match md.exif.iso_speed_ratings { Some(v) => { (*rawimg).iso          = u32::from(v) as f32; } None => {}}
  match md.exif.fnumber           { Some(v) => { (*rawimg).aperture     = v.try_into().ok().unwrap(); } None => {}}
  match md.exif.exposure_time     { Some(v) => { (*rawimg).exposure     = v.try_into().ok().unwrap(); } None => {}}
  match md.exif.focal_length      { Some(v) => { (*rawimg).focal_length = v.try_into().ok().unwrap(); } None => {}}
  match md.exif.orientation       { Some(v) => { (*rawimg).orientation  = v as u32; } None => {}}
  match md.exif.create_date       { Some(d) => { copy_string(&d, &mut (*rawimg).datetime) } None => {}}
  Ok(())
}

#[no_mangle]
pub unsafe extern "C" fn rl_decode_file(
    filename: *mut c_char,
    rawimg  : *mut c_rawimage,
    ) -> u64
{
  let c_str: &CStr = CStr::from_ptr(filename);
  let strn : &str = c_str.to_str().unwrap();
  let mut image = rawler::decode_file(strn).unwrap();
  let mut len = 0 as usize;
  if let rawler::RawImageData::Integer(ref mut vdat) = image.data
  {
    len = vdat.len();
    (*rawimg).data = vdat.as_mut_ptr() as *mut c_void;
    std::mem::forget(image.data);
    (*rawimg).data_type = 0;
  }
  else if let rawler::RawImageData::Float(ref mut vdat) = image.data
  {
    len = vdat.len();
    (*rawimg).data = vdat.as_mut_ptr() as *mut c_void;
    std::mem::forget(image.data);
    (*rawimg).data_type = 1;
  }
  else
  {
    return len as u64;
  }

  (*rawimg).width  = image.width  as u64;
  (*rawimg).stride = image.width  as u32;
  (*rawimg).height = image.height as u64;
  (*rawimg).cpp    = image.cpp    as u64;
  for k in 0..4 { (*rawimg).wb_coeffs[k]   = image.wb_coeffs[cmp::min(image.wb_coeffs.len()-1,k)]; }
  for k in 0..4 { (*rawimg).whitelevels[k] = image.whitelevel.0[cmp::min(image.whitelevel.0.len()-1,k)] as u16; }
  for k in 0..4 { (*rawimg).blacklevels[k] = image.blacklevel.levels[cmp::min(image.blacklevel.levels.len()-1,k)].as_f32() as u16; }
  // (*rawimg).orientation = image.orientation.to_u16() as u32;

  match image.color_matrix.get(&Illuminant::D65) {
    Some(m) => for j in 0..3 { for i in 0..3 { (*rawimg).xyz_to_cam[i][j] = m[3*i+j]; } }
    None    => for j in 0..3 { for i in 0..4 { (*rawimg).xyz_to_cam[i][j] = image.xyz_to_cam[i][j]; } }
  }

  copy_metadata(strn, rawimg).unwrap();

  copy_string(&image.make,  &mut (*rawimg).maker);
  copy_string(&image.model, &mut (*rawimg).model);
  copy_string(&image.clean_make,  &mut (*rawimg).clean_maker);
  copy_string(&image.clean_model, &mut (*rawimg).clean_model);

  // store aabb (x y X Y)
  // if let Rect ref cr = image.crop_area
  match image.crop_area
  {
    Some(cr) =>
    {
      (*rawimg).crop_aabb[0] =  cr.x() as u64;
      (*rawimg).crop_aabb[1] =  cr.y() as u64;
      (*rawimg).crop_aabb[2] = (cr.x() + cr.width() ) as u64;
      (*rawimg).crop_aabb[3] = (cr.y() + cr.height()) as u64;
    }
    None =>
    {
      (*rawimg).crop_aabb[0] = 0 as u64;
      (*rawimg).crop_aabb[1] = 0 as u64;
      (*rawimg).crop_aabb[2] = image.width  as u64;
      (*rawimg).crop_aabb[3] = image.height as u64;
    }
  }

  // now we need to account for the pixel shift due to an offset filter:
  let mut ox = 0 as usize;
  let mut oy = 0 as usize;

  (*rawimg).filters = 0x49494949; // something not 0 and not 9, indicating bayer
  if let RawPhotometricInterpretation::Cfa(cfa) = image.photometric
  {
    // special handling for x-trans sensors
    if cfa.width == 6
    { // find first green in same row
      (*rawimg).filters = 9;
      for i in 0..6 
      {
        if cfa.color_at(0, i) == 1
        {
          ox = i;
          break;
        }
      }
      if cfa.color_at(0, ox+1) != 1 && cfa.color_at(0, ox+2) != 1
      { // center of green x, need to go 2 down
        oy = 2;
        ox = (ox + 2) % 3;
      }
      if cfa.color_at(oy+1, ox) == 1
      { // two greens above one another, need to go down one
        oy = oy + 1;
      }
      if cfa.color_at(oy, ox+1) == 1
      { // if x+1 is green, too, either x++ or x-=2 if x>=2
        if ox >= 2 { ox = ox - 2; }
        else { ox = ox + 1; }
      }
      // now we should be at the beginning of a green 5-cross block.
      if cfa.color_at(oy, ox+1) == 2
      { // if x+1 == red and y+1 == blue, all good!
        // if not, x+=3 or y+=3, equivalently.
        if ox < oy  { ox = ox + 3; }
        else        { oy = oy + 3; }
      }
    }
    else // move to std bayer sensor offset
    {
      if cfa.color_at(0, 0) == 1
      {
        if cfa.color_at(0, 1) == 0 { ox = 1; }
        if cfa.color_at(0, 1) == 2 { oy = 1; }
      }
      else if cfa.color_at(0, 0) == 2
      {
        ox = 1;
        oy = 1;
      }
    }
    // unfortunately need to round to full 6x6 block for xtrans
    let block = if cfa.width == 6 { 3 } else { 2 } as u64;
    let bigblock = cfa.width as u64;
    // crop aabb is relative to buffer we emit,
    // so we need to move it along to the CFA pattern boundary
    let ref mut b = (*rawimg).crop_aabb;
    b[2] -= ox as u64;
    b[3] -= oy as u64;
    // and also we'll round it to cut only between CFA blocks
    b[0] = ((b[0] + bigblock - 1) / bigblock) * bigblock;
    b[1] = ((b[1] + bigblock - 1) / bigblock) * bigblock;
    b[2] =  (b[2] / block) * block;
    b[3] =  (b[3] / block) * block;

    (*rawimg).width  -= ox as u64;
    (*rawimg).height -= oy as u64;
    // round down to full block size:
    (*rawimg).width  = ((*rawimg).width /block)*block;
    (*rawimg).height = ((*rawimg).height/block)*block;
  }
  (*rawimg).cfa_off_x = ox as u32;
  (*rawimg).cfa_off_y = oy as u32;
  len as u64
}

#[no_mangle]
pub unsafe extern "C" fn rl_deallocate(ptr: *mut c_void, len: u64)
{
  let len = len as usize;
  drop(Vec::from_raw_parts(ptr, len, len));
}
