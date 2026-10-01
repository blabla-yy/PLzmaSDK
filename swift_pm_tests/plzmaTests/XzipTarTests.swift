import Testing
import Foundation
@testable import PLzmaSDK

// xzip additions to tar handling: an AIX symlink header the handler used to stop at, the OS error
// number carried in a failed open / mkdir's reason, and symbolic links restored as links - inside
// the extraction directory only. Tars are built in memory; nothing here needs a fixture file.

private enum TarBytes {
    static func entry(_ name: String, type: Character = "0", content: Data = Data(), link: String = "",
                      mtimeField: [UInt8]? = nil) -> Data {
        var header = [UInt8](repeating: 0, count: 512)
        func put(_ bytes: [UInt8], at offset: Int) {
            header.replaceSubrange(offset..<(offset + bytes.count), with: bytes)
        }
        func octal(_ value: Int, width: Int) -> [UInt8] {
            let digits = String(value, radix: 8)
            return Array((String(repeating: "0", count: max(0, width - 1 - digits.count)) + digits + "\0").utf8)
        }
        put(Array(name.utf8.prefix(100)), at: 0)
        put(octal(type == "5" ? 0o755 : 0o644, width: 8), at: 100)
        put(octal(0, width: 8), at: 108)
        put(octal(0, width: 8), at: 116)
        put(octal(content.count, width: 12), at: 124)
        put(mtimeField ?? octal(1_700_000_000, width: 12), at: 136)
        put([UInt8](repeating: 0x20, count: 8), at: 148)
        header[156] = type.asciiValue ?? 0x30
        put(Array(link.utf8.prefix(100)), at: 157)
        put(Array("ustar\0".utf8) + [0x30, 0x30], at: 257)
        let digits = String(header.reduce(0) { $0 + Int($1) }, radix: 8)
        put(Array((String(repeating: "0", count: max(0, 6 - digits.count)) + digits + "\0 ").utf8), at: 148)
        var out = Data(header)
        out.append(content)
        out.append(Data(count: (512 - content.count % 512) % 512))
        return out
    }

    static func archive(_ entries: [Data]) -> Data {
        entries.reduce(Data()) { $0 + $1 } + Data(count: 1024)
    }
}

private func scratch() throws -> URL {
    let dir = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("plzma-tar-\(UUID().uuidString)")
    try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    return dir
}

private func openTar(_ data: Data, in dir: URL) throws -> Decoder {
    let url = dir.appendingPathComponent("test.tar")
    try data.write(to: url)
    let decoder = try Decoder(stream: InStream(path: Path(url.path)), fileType: .tar, delegate: nil)
    try #require(try decoder.open(), "the tar did not open")
    return decoder
}

private func paths(_ decoder: Decoder) throws -> [String] {
    let items = try decoder.items()
    return try (0..<items.count).map { try items.item(at: $0).path().description }
}

@Suite struct XzipTarHeaderTests {

    /// AIX tar writes a symlink's mtime as NUL followed by digits. The handler read that as a
    /// malformed number, called the header invalid and ended the listing at the link: a directory,
    /// a symlink and a file became just the directory.
    @Test func aSymlinkWhoseMtimeStartsWithNulStillListsTheRest() throws {
        let dir = try scratch()
        defer { try? FileManager.default.removeItem(at: dir) }
        let nulMtime: [UInt8] = [0] + Array("1722000726 ".utf8)
        let tar = TarBytes.archive([
            TarBytes.entry("sample/", type: "5"),
            TarBytes.entry("sample/link", type: "2", link: "text-file.txt", mtimeField: nulMtime),
            TarBytes.entry("sample/text-file.txt", content: Data("t".utf8)),
        ])
        let decoder = try openTar(tar, in: dir)
        #expect(try paths(decoder) == ["sample", "sample/link", "sample/text-file.txt"])
    }
}

@Suite(.serialized) struct XzipErrnoReasonTests {

    private func extractionFailure(_ tar: Data, into destination: URL, dir: URL) throws -> Exception? {
        let decoder = try openTar(tar, in: dir)
        do {
            _ = try decoder.extract(to: Path(destination.path))
            Issue.record("extraction did not fail")
            return nil
        } catch let exception as Exception {
            return exception
        }
    }

    @Test func aNameTooLongForTheFilesystemSaysSoInsteadOfBlamingPermissions() throws {
        let dir = try scratch()
        defer { try? FileManager.default.removeItem(at: dir) }
        let out = dir.appendingPathComponent("out")
        try FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)
        let tar = TarBytes.archive([TarBytes.entry(String(repeating: "n", count: 99), content: Data("x".utf8))])
        // A 99-byte name fits the tar; make the destination so deep that the whole path does not fit.
        var deep = out
        for _ in 0..<12 { deep = deep.appendingPathComponent(String(repeating: "d", count: 100)) }
        try FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)
        // mkdir of the deep path is what fails (ENAMETOOLONG once past PATH_MAX).
        let exception = try #require(try extractionFailure(tar, into: deep, dir: dir))
        #expect(exception.code == .io)
        #expect(exception.reason.hasPrefix("errno:\(ENAMETOOLONG) "), "\(exception.reason)")
    }

    @Test func aDeniedWriteKeepsTheOldSentenceAndAddsTheErrno() throws {
        let dir = try scratch()
        let out = dir.appendingPathComponent("readonly")
        try FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)
        try FileManager.default.setAttributes([.posixPermissions: 0o500], ofItemAtPath: out.path)
        defer {
            try? FileManager.default.setAttributes([.posixPermissions: 0o700], ofItemAtPath: out.path)
            try? FileManager.default.removeItem(at: dir)
        }
        let tar = TarBytes.archive([TarBytes.entry("f.txt", content: Data("x".utf8))])
        let exception = try #require(try extractionFailure(tar, into: out, dir: dir))
        #expect(exception.reason.hasPrefix("errno:\(EACCES) "), "\(exception.reason)")
        #expect(exception.reason.contains("write permission"))
    }
}

@Suite(.serialized) struct XzipSymlinkTests {

    private func extract(_ entries: [Data]) throws -> (root: URL, out: URL) {
        let root = try scratch()
        let out = root.appendingPathComponent("out")
        try FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)
        let decoder = try openTar(TarBytes.archive(entries), in: root)
        _ = try decoder.extract(to: Path(out.path))
        return (root, out)
    }

    private func linkTarget(_ url: URL) -> String? {
        try? FileManager.default.destinationOfSymbolicLink(atPath: url.path)
    }

    @Test func aLinkInsideTheDestinationIsRestoredAsALink() throws {
        let (root, out) = try extract([
            TarBytes.entry("target.txt", content: Data("hello".utf8)),
            TarBytes.entry("d/", type: "5"),
            TarBytes.entry("d/up", type: "2", link: "../target.txt"),
            TarBytes.entry("same", type: "2", link: "target.txt"),
        ])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(linkTarget(out.appendingPathComponent("d/up")) == "../target.txt")
        #expect(linkTarget(out.appendingPathComponent("same")) == "target.txt")
        #expect(try String(contentsOf: out.appendingPathComponent("d/up"), encoding: .utf8) == "hello")
    }

    @Test(arguments: ["/etc/hosts", "../outside", "a/../../outside", ".."])
    func aLinkThatCouldLeaveTheDestinationIsNotCreated(_ target: String) throws {
        let (root, out) = try extract([TarBytes.entry("link", type: "2", link: target)])
        defer { try? FileManager.default.removeItem(at: root) }
        #expect(linkTarget(out.appendingPathComponent("link")) == nil, "\(target) was restored as a link")
        // What the old behaviour left there: a regular file holding the target text.
        #expect(try String(contentsOf: out.appendingPathComponent("link"), encoding: .utf8) == target)
    }

    @Test func nothingIsWrittenOutsideThroughAnEarlierLink() throws {
        let root = try scratch()
        defer { try? FileManager.default.removeItem(at: root) }
        let out = root.appendingPathComponent("out")
        try FileManager.default.createDirectory(at: out, withIntermediateDirectories: true)
        let decoder = try openTar(TarBytes.archive([
            TarBytes.entry("s1", type: "2", link: "t1/.."),
            TarBytes.entry("t1", type: "2", link: "."),
            TarBytes.entry("s1/pwned.txt", content: Data("pwn".utf8)),
        ]), in: root)
        _ = try? decoder.extract(to: Path(out.path))
        #expect(!FileManager.default.fileExists(atPath: root.appendingPathComponent("pwned.txt").path))
    }
}
