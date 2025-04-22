variable "PLATFORM" {
  default = replace(BAKE_LOCAL_PLATFORM, "/", "-")
}

target "n00b" {
  cache-from = ["type=registry,ref=ghcr.io/crashappsec/n00b_compile:cache-${PLATFORM}"]
  cache-to   = ["type=registry,ref=ghcr.io/crashappsec/n00b_compile:cache-${PLATFORM}"]
}
