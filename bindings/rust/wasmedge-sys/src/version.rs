use super::wasmedge;
use std::ffi::CStr;

/// # WasmEdge Version
///
/// ## Example
///
/// ```rust
/// // in `wasmedge.rs`
/// pub const WASMEDGE_VERSION: &'static [u8; 22usize] = b"0.8.2-rc.5-1-g809c746\0";
/// pub const WASMEDGE_VERSION_MAJOR: u32 = 0;
/// pub const WASMEDGE_VERSION_MINOR: u32 = 8;
/// pub const WASMEDGE_VERSION_PATCH: u32 = 2;
/// ```
pub fn full_version() -> &'static str {
    let valid_version =
        CStr::from_bytes_with_nul(wasmedge::WASMEDGE_VERSION).expect("CString::new failed");
    valid_version.to_str().expect("to_str() call failed")
}

pub fn semv_version() -> String {
    format!(
        "{}.{}.{}",
        wasmedge::WASMEDGE_VERSION_MAJOR,
        wasmedge::WASMEDGE_VERSION_MINOR,
        wasmedge::WASMEDGE_VERSION_PATCH
    )
}
