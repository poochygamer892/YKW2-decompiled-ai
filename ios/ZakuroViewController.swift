import AVFoundation
import UIKit

/// iOS frontend for the same Zakuro runtime used by the Android build.
final class ZakuroViewController: UIViewController {
    private let romURL: URL
    private let dataURL: URL
    private let statusLabel = UILabel()
    private let topScreen = UIImageView()
    private let bottomScreen = UIImageView()
    private var displayLink: CADisplayLink?
    private var buttons: UInt32 = 0
    private var touchPoint: CGPoint?
    private let audioEngine = AVAudioEngine()
    private let audioPlayer = AVAudioPlayerNode()
    private lazy var audioFormat = AVAudioFormat(
        standardFormatWithSampleRate: 32_728,
        channels: 2
    )!

    init(romURL: URL, dataURL: URL) {
        self.romURL = romURL
        self.dataURL = dataURL
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("ZakuroViewController does not use storyboards")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Yo-kai Watch 2"
        view.backgroundColor = .black
        buildInterface()
        configureAudio()
        statusLabel.text = "Abriendo la ROM con Zakuro…"

        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self else { return }
            let error = YKW2ImportBridge.openZakuroROM(at: self.romURL, dataDirectoryURL: self.dataURL)
            DispatchQueue.main.async {
                if error.isEmpty {
                    self.statusLabel.text = "Zakuro · Yo-kai Watch 2"
                    self.startFrameLoop()
                } else {
                    self.statusLabel.text = "No se pudo iniciar Zakuro: \(error)"
                }
            }
        }
    }

    override func viewWillDisappear(_ animated: Bool) {
        displayLink?.invalidate()
        displayLink = nil
        audioPlayer.stop()
        audioEngine.stop()
        YKW2ImportBridge.closeZakuro()
        super.viewWillDisappear(animated)
    }

    private func buildInterface() {
        statusLabel.textColor = .white
        statusLabel.textAlignment = .center
        statusLabel.numberOfLines = 2
        statusLabel.font = .preferredFont(forTextStyle: .footnote)

        for screen in [topScreen, bottomScreen] {
            screen.backgroundColor = UIColor(white: 0.07, alpha: 1)
            screen.contentMode = .scaleAspectFit
            screen.clipsToBounds = true
            screen.layer.cornerRadius = 8
        }
        bottomScreen.isUserInteractionEnabled = true
        let touchGesture = UILongPressGestureRecognizer(target: self, action: #selector(touchScreen(_:)))
        touchGesture.minimumPressDuration = 0
        touchGesture.cancelsTouchesInView = false
        bottomScreen.addGestureRecognizer(touchGesture)

        let controls = UIStackView(arrangedSubviews: [
            controlRow([("↑", 1 << 6), ("A", 1 << 0), ("B", 1 << 1), ("START", 1 << 3)]),
            controlRow([("←", 1 << 5), ("↓", 1 << 7), ("→", 1 << 4)]),
        ])
        controls.axis = .vertical
        controls.spacing = 8

        let stack = UIStackView(arrangedSubviews: [statusLabel, topScreen, bottomScreen, controls])
        stack.axis = .vertical
        stack.spacing = 10
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 12),
            stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -12),
            stack.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor, constant: 8),
            stack.bottomAnchor.constraint(lessThanOrEqualTo: view.safeAreaLayoutGuide.bottomAnchor, constant: -8),
            topScreen.heightAnchor.constraint(equalTo: topScreen.widthAnchor, multiplier: 0.60),
            bottomScreen.heightAnchor.constraint(equalTo: bottomScreen.widthAnchor, multiplier: 0.75),
        ])
    }

    private func controlRow(_ definitions: [(String, UInt32)]) -> UIStackView {
        let row = UIStackView()
        row.axis = .horizontal
        row.distribution = .fillEqually
        row.spacing = 8
        for (title, mask) in definitions {
            let button = UIButton(type: .system)
            button.setTitle(title, for: .normal)
            button.titleLabel?.font = .systemFont(ofSize: 18, weight: .semibold)
            button.setTitleColor(.white, for: .normal)
            button.backgroundColor = UIColor(red: 0.10, green: 0.30, blue: 0.38, alpha: 1)
            button.layer.cornerRadius = 10
            button.tag = Int(mask)
            button.addTarget(self, action: #selector(buttonDown(_:)), for: .touchDown)
            button.addTarget(self, action: #selector(buttonUp(_:)), for: [.touchUpInside, .touchUpOutside, .touchCancel])
            row.addArrangedSubview(button)
            button.heightAnchor.constraint(equalToConstant: 48).isActive = true
        }
        return row
    }

    private func configureAudio() {
        audioEngine.attach(audioPlayer)
        audioEngine.connect(audioPlayer, to: audioEngine.mainMixerNode, format: audioFormat)
        do {
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .game)
            try AVAudioSession.sharedInstance().setActive(true)
            try audioEngine.start()
            audioPlayer.play()
        } catch {
            statusLabel.text = "El audio no está disponible: \(error.localizedDescription)"
        }
    }

    private func startFrameLoop() {
        displayLink?.invalidate()
        displayLink = CADisplayLink(target: self, selector: #selector(advanceFrame))
        displayLink?.add(to: .main, forMode: .common)
    }

    @objc private func buttonDown(_ sender: UIButton) {
        buttons |= UInt32(sender.tag)
    }

    @objc private func buttonUp(_ sender: UIButton) {
        buttons &= ~UInt32(sender.tag)
    }

    @objc private func touchScreen(_ recognizer: UIGestureRecognizer) {
        let point = recognizer.location(in: bottomScreen)
        let scale = min(bottomScreen.bounds.width / 320, bottomScreen.bounds.height / 240)
        let width = 320 * scale
        let height = 240 * scale
        let left = (bottomScreen.bounds.width - width) / 2
        let top = (bottomScreen.bounds.height - height) / 2
        if recognizer.state == .ended || recognizer.state == .cancelled {
            touchPoint = nil
        } else if point.x >= left, point.x < left + width, point.y >= top, point.y < top + height {
            touchPoint = CGPoint(x: (point.x - left) / scale, y: (point.y - top) / scale)
        }
    }

    @objc private func advanceFrame() {
        let point = touchPoint
        _ = YKW2ImportBridge.setZakuroButtons(
            buttons,
            touchX: point.map { Int($0.x) } ?? -1,
            touchY: point.map { Int($0.y) } ?? -1
        )
        let outcome = YKW2ImportBridge.runZakuroFrames(1)
        guard outcome == 1 else {
            displayLink?.invalidate()
            statusLabel.text = "Zakuro detuvo la emulación (estado \(outcome))."
            return
        }
        if let top = image(fromRGBA: YKW2ImportBridge.zakuroTopScreenRGBA(), width: 400, height: 240) {
            topScreen.image = top
        }
        if let bottom = image(fromRGBA: YKW2ImportBridge.zakuroBottomScreenRGBA(), width: 320, height: 240) {
            bottomScreen.image = bottom
        }
        playAudio(YKW2ImportBridge.takeZakuroAudioFrames(2048))
    }

    private func image(fromRGBA data: Data, width: Int, height: Int) -> UIImage? {
        guard data.count == width * height * 4,
              let provider = CGDataProvider(data: data as CFData),
              let cgImage = CGImage(
                width: width,
                height: height,
                bitsPerComponent: 8,
                bitsPerPixel: 32,
                bytesPerRow: width * 4,
                space: CGColorSpaceCreateDeviceRGB(),
                bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue).union(.byteOrder32Big),
                provider: provider,
                decode: nil,
                shouldInterpolate: false,
                intent: .defaultIntent
              ) else { return nil }
        return UIImage(cgImage: cgImage)
    }

    private func playAudio(_ data: Data) {
        let sampleCount = data.count / MemoryLayout<Int16>.size
        let frameCount = sampleCount / 2
        guard frameCount > 0,
              let buffer = AVAudioPCMBuffer(pcmFormat: audioFormat, frameCapacity: AVAudioFrameCount(frameCount)),
              let channels = buffer.floatChannelData else { return }
        data.withUnsafeBytes { raw in
            let samples = raw.bindMemory(to: Int16.self)
            for frame in 0..<frameCount {
                channels[0][frame] = Float(samples[frame * 2]) / Float(Int16.max)
                channels[1][frame] = Float(samples[frame * 2 + 1]) / Float(Int16.max)
            }
        }
        buffer.frameLength = AVAudioFrameCount(frameCount)
        audioPlayer.scheduleBuffer(buffer)
    }
}
