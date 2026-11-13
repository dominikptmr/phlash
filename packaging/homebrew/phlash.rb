# template filled by scripts/update-formula.sh
class Phlash < Formula
  desc "Instant keyboard navigation for macOS Spaces, with caps lock as Hyper"
  homepage "https://github.com/dominikptmr/phlash"
  url "@URL@"
  sha256 "@SHA256@"
  license "MIT"
  head "https://github.com/dominikptmr/phlash.git", branch: "main"

  depends_on "cmake" => :build
  depends_on :macos

  def install
    system "cmake", "-S", ".", "-B", "build", "-DPHLASH_SIGN_IDENTITY=-", *std_cmake_args
    system "cmake", "--build", "build"
    bin.install "build/phlash"
  end

  service do
    run opt_bin/"phlash"
    keep_alive crashed: true
    process_type :interactive
    environment_variables PATH: std_service_path_env
    log_path var/"log/phlash.log"
    error_log_path var/"log/phlash.log"
  end

  def caveats
    <<~EOS
      Start phlash now and at every login:
        brew services start phlash

      macOS asks for Accessibility permission on first start: enable phlash in
      System Settings → Privacy & Security → Accessibility. phlash waits and
      starts working as soon as it's granted. After `brew upgrade phlash`,
      macOS may ask again.

      On first start phlash creates an empty ~/.config/phlash/config.
      Syntax: https://github.com/dominikptmr/phlash/blob/main/docs/configuration.md
      Reload it after editing:
        pkill -HUP phlash

      Before starting, quit other caps lock remappers (Hyperkey, Karabiner
      caps lock rules) and turn off System Settings → Desktop & Dock →
      "Automatically rearrange Spaces based on most recent use".

      Caps lock must not be set to "No Action": System Settings → Keyboard →
      Keyboard Shortcuts… → Modifier Keys, for each keyboard set Caps Lock to
      ⇪ Caps Lock. Otherwise macOS drops the key before phlash sees it.

      Log: #{var}/log/phlash.log
    EOS
  end

  test do
    (testpath/".config/phlash").mkpath
    (testpath/".config/phlash/config").write <<~EOS
      hyper : space back
      hyper - s : app Finder
    EOS
    assert_match "is valid", shell_output("HOME=#{testpath} #{bin}/phlash --check")
  end
end
