import AVFoundation
import UIKit

/// Full-screen iOS host for Zakuro, with a configurable on-screen 3DS gamepad.
final class ZakuroViewController: UIViewController {
    private let romURL: URL
    private let dataURL: URL
    private let statusLabel = UILabel()
    private let topScreen = UIImageView()
    private let bottomScreen = UIImageView()
    private let screenStack = UIStackView()
    private let controlsOverlay = PassthroughControlsView()
    private let editorPanel = UIVisualEffectView(effect: UIBlurEffect(style: .systemChromeMaterialDark))
    private let selectedControlButton = UIButton(type: .system)
    private let sizeSlider = UISlider()
    private let selectionText = UILabel()
    private var displayLink: CADisplayLink?
    private let runtimeQueue = DispatchQueue(label: "com.ykw2.port.runtime", qos: .userInteractive)
    private let audioQueue = DispatchQueue(label: "com.ykw2.port.audio", qos: .userInitiated)
    private var frameInFlight = false
    private var buttonsDown = Set<String>()
    private var circleX: Float = 0
    private var circleY: Float = 0
    private var touchPoint: CGPoint?
    private var controlButtons: [String: UIButton] = [:]
    private var selectedControl = "ALL"
    private var editorMode = false
    private var layoutProfile = ""
    private var portraitScreenConstraints: [NSLayoutConstraint] = []
    private var landscapeScreenConstraints: [NSLayoutConstraint] = []
    private var controlPositions: [String: CGPoint] = [:]
    private var controlSizes: [String: CGFloat] = [:]
    private var globalControlScale: CGFloat = 1
    private let audioEngine = AVAudioEngine()
    private let audioPlayer = AVAudioPlayerNode()
    private lazy var audioFormat = AVAudioFormat(standardFormatWithSampleRate: 32_728, channels: 2)!

    private let controlDefinitions: [(key: String, title: String, bit: UInt32)] = [
        ("UP", "↑", 1 << 6), ("DOWN", "↓", 1 << 7),
        ("LEFT", "←", 1 << 5), ("RIGHT", "→", 1 << 4),
        ("A", "A", 1 << 0), ("B", "B", 1 << 1),
        ("X", "X", 1 << 10), ("Y", "Y", 1 << 11),
        ("SELECT", "SEL", 1 << 2), ("START", "START", 1 << 3),
        ("L", "L", 1 << 9), ("R", "R", 1 << 8),
    ]

    init(romURL: URL, dataURL: URL) {
        self.romURL = romURL
        self.dataURL = dataURL
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("ZakuroViewController does not use storyboards")
    }

    override var prefersStatusBarHidden: Bool { true }
    override var preferredScreenEdgesDeferringSystemGestures: UIRectEdge { .all }

    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        navigationController?.setNavigationBarHidden(true, animated: animated)
        setNeedsStatusBarAppearanceUpdate()
    }

    override func viewWillDisappear(_ animated: Bool) {
        if isMovingFromParent || isBeingDismissed {
            displayLink?.invalidate()
            displayLink = nil
            audioPlayer.stop()
            audioEngine.stop()
            YKW2ImportBridge.closeZakuro()
            navigationController?.setNavigationBarHidden(false, animated: animated)
        }
        super.viewWillDisappear(animated)
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
                    self.statusLabel.text = ""
                    self.startFrameLoop()
                } else {
                    self.statusLabel.text = "No se pudo iniciar Zakuro: \(error)"
                }
            }
        }
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()
        let nextProfile = view.bounds.height >= view.bounds.width ? "portrait" : "landscape"
        if nextProfile != layoutProfile {
            layoutProfile = nextProfile
            screenStack.axis = nextProfile == "portrait" ? .vertical : .horizontal
            NSLayoutConstraint.deactivate(portraitScreenConstraints + landscapeScreenConstraints)
            NSLayoutConstraint.activate(nextProfile == "portrait" ? portraitScreenConstraints : landscapeScreenConstraints)
            loadControlLayout()
        }
        layoutTouchControls()
    }

    private func buildInterface() {
        statusLabel.textColor = .white
        statusLabel.textAlignment = .center
        statusLabel.numberOfLines = 2
        statusLabel.font = .preferredFont(forTextStyle: .footnote)
        statusLabel.backgroundColor = UIColor.black.withAlphaComponent(0.62)
        statusLabel.layer.cornerRadius = 8
        statusLabel.clipsToBounds = true

        for screen in [topScreen, bottomScreen] {
            screen.backgroundColor = UIColor(white: 0.07, alpha: 1)
            screen.contentMode = .scaleAspectFit
            screen.clipsToBounds = true
        }
        screenStack.axis = .vertical
        screenStack.spacing = 0
        screenStack.translatesAutoresizingMaskIntoConstraints = false
        screenStack.addArrangedSubview(topScreen)
        screenStack.addArrangedSubview(bottomScreen)
        view.addSubview(screenStack)
        let preferredScreenWidth = screenStack.widthAnchor.constraint(equalTo: view.widthAnchor, multiplier: 0.96)
        preferredScreenWidth.priority = .defaultHigh
        NSLayoutConstraint.activate([
            screenStack.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            screenStack.centerYAnchor.constraint(equalTo: view.centerYAnchor),
            preferredScreenWidth,
            screenStack.widthAnchor.constraint(lessThanOrEqualTo: view.widthAnchor, multiplier: 0.96),
            screenStack.heightAnchor.constraint(lessThanOrEqualTo: view.heightAnchor, multiplier: 0.84),
        ])
        portraitScreenConstraints = [
            topScreen.widthAnchor.constraint(equalTo: screenStack.widthAnchor),
            topScreen.heightAnchor.constraint(equalTo: screenStack.widthAnchor, multiplier: 0.60),
            bottomScreen.widthAnchor.constraint(equalTo: screenStack.widthAnchor),
            bottomScreen.heightAnchor.constraint(equalTo: screenStack.widthAnchor, multiplier: 0.75),
        ]
        landscapeScreenConstraints = [
            topScreen.heightAnchor.constraint(equalTo: screenStack.heightAnchor),
            topScreen.widthAnchor.constraint(equalTo: screenStack.heightAnchor, multiplier: 5.0 / 3.0),
            bottomScreen.heightAnchor.constraint(equalTo: screenStack.heightAnchor),
            bottomScreen.widthAnchor.constraint(equalTo: screenStack.heightAnchor, multiplier: 4.0 / 3.0),
        ]
        portraitScreenConstraints.forEach { $0.isActive = false }
        landscapeScreenConstraints.forEach { $0.isActive = false }
        bottomScreen.isUserInteractionEnabled = true
        let screenTouch = UILongPressGestureRecognizer(target: self, action: #selector(touchScreen(_:)))
        screenTouch.minimumPressDuration = 0
        screenTouch.cancelsTouchesInView = false
        bottomScreen.addGestureRecognizer(screenTouch)

        controlsOverlay.backgroundColor = .clear
        controlsOverlay.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(controlsOverlay)
        NSLayoutConstraint.activate([
            controlsOverlay.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            controlsOverlay.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            controlsOverlay.topAnchor.constraint(equalTo: view.topAnchor),
            controlsOverlay.bottomAnchor.constraint(equalTo: view.bottomAnchor),
        ])

        for definition in controlDefinitions {
            let button = UIButton(type: .system)
            button.setTitle(definition.title, for: .normal)
            button.titleLabel?.font = .systemFont(ofSize: 17, weight: .bold)
            button.setTitleColor(.white, for: .normal)
            button.backgroundColor = UIColor(red: 0.15, green: 0.18, blue: 0.22, alpha: 0.78)
            button.layer.borderColor = UIColor(red: 0.63, green: 0.70, blue: 0.76, alpha: 0.86).cgColor
            button.layer.borderWidth = 2
            button.layer.cornerRadius = 22
            button.accessibilityLabel = definition.key
            button.addTarget(self, action: #selector(controlPressed(_:)), for: .touchDown)
            button.addTarget(self, action: #selector(controlReleased(_:)), for: [.touchUpInside, .touchUpOutside, .touchCancel])
            let drag = UIPanGestureRecognizer(target: self, action: #selector(dragControl(_:)))
            drag.delegate = self
            button.addGestureRecognizer(drag)
            controlsOverlay.addSubview(button)
            controlButtons[definition.key] = button
        }

        let joystick = AnalogStickControl()
        joystick.accessibilityLabel = "Joystick"
        joystick.onAxisChanged = { [weak self] x, y in
            self?.circleX = x
            self?.circleY = y
        }
        let joystickDrag = UIPanGestureRecognizer(target: self, action: #selector(dragControl(_:)))
        joystickDrag.delegate = self
        joystick.addGestureRecognizer(joystickDrag)
        controlsOverlay.addSubview(joystick)

        let back = chromeButton("‹")
        back.titleLabel?.font = .systemFont(ofSize: 30, weight: .medium)
        back.addTarget(self, action: #selector(goBack), for: .touchUpInside)
        controlsOverlay.addSubview(back)
        back.accessibilityIdentifier = "BACK"

        let edit = chromeButton("⚙")
        edit.addTarget(self, action: #selector(toggleEditor), for: .touchUpInside)
        controlsOverlay.addSubview(edit)
        edit.accessibilityIdentifier = "EDITOR"

        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        controlsOverlay.addSubview(statusLabel)
        NSLayoutConstraint.activate([
            statusLabel.centerXAnchor.constraint(equalTo: controlsOverlay.centerXAnchor),
            statusLabel.topAnchor.constraint(equalTo: controlsOverlay.safeAreaLayoutGuide.topAnchor, constant: 4),
            statusLabel.widthAnchor.constraint(lessThanOrEqualTo: controlsOverlay.widthAnchor, multiplier: 0.72),
        ])

        buildEditorPanel()
        loadControlLayout()
    }

    private func chromeButton(_ title: String) -> UIButton {
        let button = UIButton(type: .system)
        button.setTitle(title, for: .normal)
        button.setTitleColor(.white, for: .normal)
        button.titleLabel?.font = .systemFont(ofSize: 19, weight: .semibold)
        button.backgroundColor = UIColor.black.withAlphaComponent(0.55)
        button.layer.cornerRadius = 18
        return button
    }

    private func buildEditorPanel() {
        editorPanel.translatesAutoresizingMaskIntoConstraints = false
        editorPanel.layer.cornerRadius = 14
        editorPanel.clipsToBounds = true
        controlsOverlay.addSubview(editorPanel)

        let instructions = UILabel()
        instructions.text = "Elige un control y ajusta su tamaño · arrástralo para moverlo"
        instructions.textColor = .white
        instructions.font = .systemFont(ofSize: 12, weight: .medium)
        instructions.textAlignment = .center
        instructions.numberOfLines = 2

        selectedControlButton.setTitle("Tamaño · Todos ▾", for: .normal)
        selectedControlButton.setTitleColor(.white, for: .normal)
        selectedControlButton.titleLabel?.font = .systemFont(ofSize: 14, weight: .semibold)
        selectedControlButton.addTarget(self, action: #selector(chooseControl), for: .touchUpInside)

        selectionText.text = "Todos · 100%"
        selectionText.textColor = UIColor(white: 0.85, alpha: 1)
        selectionText.textAlignment = .center
        selectionText.font = .systemFont(ofSize: 11)

        sizeSlider.minimumValue = 0.5
        sizeSlider.maximumValue = 2
        sizeSlider.value = 1
        sizeSlider.addTarget(self, action: #selector(changeControlSize), for: .valueChanged)

        let reset = UIButton(type: .system)
        reset.setTitle("Restablecer", for: .normal)
        reset.addTarget(self, action: #selector(resetControls), for: .touchUpInside)
        let done = UIButton(type: .system)
        done.setTitle("Listo", for: .normal)
        done.addTarget(self, action: #selector(toggleEditor), for: .touchUpInside)
        let actions = UIStackView(arrangedSubviews: [reset, done])
        actions.axis = .horizontal
        actions.distribution = .fillEqually

        let content = UIStackView(arrangedSubviews: [instructions, selectedControlButton, selectionText, sizeSlider, actions])
        content.axis = .vertical
        content.spacing = 4
        content.translatesAutoresizingMaskIntoConstraints = false
        editorPanel.contentView.addSubview(content)
        NSLayoutConstraint.activate([
            content.leadingAnchor.constraint(equalTo: editorPanel.contentView.leadingAnchor, constant: 10),
            content.trailingAnchor.constraint(equalTo: editorPanel.contentView.trailingAnchor, constant: -10),
            content.topAnchor.constraint(equalTo: editorPanel.contentView.topAnchor, constant: 6),
            content.bottomAnchor.constraint(equalTo: editorPanel.contentView.bottomAnchor, constant: -6),
            editorPanel.centerXAnchor.constraint(equalTo: controlsOverlay.centerXAnchor),
            editorPanel.topAnchor.constraint(equalTo: controlsOverlay.safeAreaLayoutGuide.topAnchor, constant: 8),
            editorPanel.widthAnchor.constraint(lessThanOrEqualTo: controlsOverlay.widthAnchor, multiplier: 0.78),
            editorPanel.widthAnchor.constraint(greaterThanOrEqualToConstant: 230),
        ])
        editorPanel.isHidden = true
    }

    private func loadControlLayout() {
        guard !layoutProfile.isEmpty else { return }
        let defaults: [String: CGPoint] = [
            "UP": CGPoint(x: 0.115, y: 0.51), "DOWN": CGPoint(x: 0.115, y: 0.67),
            "LEFT": CGPoint(x: 0.085, y: 0.59), "RIGHT": CGPoint(x: 0.185, y: 0.59),
            "STICK": CGPoint(x: 0.15, y: 0.84),
            "X": CGPoint(x: 0.88, y: 0.56), "Y": CGPoint(x: 0.80, y: 0.65),
            "A": CGPoint(x: 0.92, y: 0.65), "B": CGPoint(x: 0.88, y: 0.74),
            "SELECT": CGPoint(x: 0.80, y: 0.45), "START": CGPoint(x: 0.93, y: 0.45),
            "L": CGPoint(x: 0.09, y: 0.11), "R": CGPoint(x: 0.91, y: 0.11),
        ]
        let preferences = UserDefaults.standard
        let prefix = "ykw2.controls.\(layoutProfile)."
        controlPositions = defaults.mapValues { $0 }
        for key in defaults.keys {
            let x = preferences.object(forKey: prefix + key + ".x") as? Double
            let y = preferences.object(forKey: prefix + key + ".y") as? Double
            if let x, let y { controlPositions[key] = CGPoint(x: x, y: y) }
        }
        controlSizes = Dictionary(uniqueKeysWithValues: defaults.keys.map { key in
            (key, CGFloat(preferences.object(forKey: prefix + key + ".size") as? Double ?? 1))
        })
        globalControlScale = CGFloat(preferences.object(forKey: prefix + "globalSize") as? Double ?? 1)
        setNeedsStatusBarAppearanceUpdate()
    }

    private func saveControlLayout() {
        let preferences = UserDefaults.standard
        let prefix = "ykw2.controls.\(layoutProfile)."
        for (key, point) in controlPositions {
            preferences.set(Double(point.x), forKey: prefix + key + ".x")
            preferences.set(Double(point.y), forKey: prefix + key + ".y")
            preferences.set(Double(controlSizes[key] ?? 1), forKey: prefix + key + ".size")
        }
        preferences.set(Double(globalControlScale), forKey: prefix + "globalSize")
    }

    private func layoutTouchControls() {
        guard controlsOverlay.bounds.width > 0, controlsOverlay.bounds.height > 0 else { return }
        let width = controlsOverlay.bounds.width
        let height = controlsOverlay.bounds.height
        let base = min(56, max(40, min(width, height) * 0.14)) * globalControlScale
        for definition in controlDefinitions {
            guard let button = controlButtons[definition.key] else { continue }
            let center = controlPositions[definition.key] ?? CGPoint(x: 0.85, y: 0.7)
            let size = base * (controlSizes[definition.key] ?? 1)
            let isShoulder = definition.key == "L" || definition.key == "R"
            let isUtility = definition.key == "SELECT" || definition.key == "START"
            let frameSize = isShoulder || isUtility ? CGSize(width: size * (definition.key == "START" ? 1.65 : 1.2), height: size * 0.74) : CGSize(width: size, height: size)
            button.bounds = CGRect(origin: .zero, size: frameSize)
            button.center = CGPoint(x: width * center.x, y: height * center.y)
            button.layer.cornerRadius = isShoulder || isUtility ? frameSize.height / 2 : frameSize.width / 2
            button.titleLabel?.font = .systemFont(ofSize: min(20, max(11, frameSize.height * 0.35)), weight: .bold)
        }
        if let stick = controlsOverlay.subviews.compactMap({ $0 as? AnalogStickControl }).first {
            let p = controlPositions["STICK"] ?? CGPoint(x: 0.15, y: 0.84)
            let diameter = base * 3.4 * (controlSizes["STICK"] ?? 1)
            stick.bounds = CGRect(x: 0, y: 0, width: diameter, height: diameter)
            stick.center = CGPoint(x: width * p.x, y: height * p.y)
            stick.setNeedsDisplay()
        }
        for button in controlsOverlay.subviews.compactMap({ $0 as? UIButton }) {
            switch button.accessibilityIdentifier {
            case "BACK":
                button.frame = CGRect(x: 12, y: controlsOverlay.safeAreaInsets.top + 8, width: 42, height: 36)
            case "EDITOR":
                button.frame = CGRect(x: width - 54, y: controlsOverlay.safeAreaInsets.top + 8, width: 42, height: 36)
            default: break
            }
        }
    }

    @objc private func touchScreen(_ recognizer: UIGestureRecognizer) {
        let point = recognizer.location(in: bottomScreen)
        if recognizer.state == .ended || recognizer.state == .cancelled || recognizer.state == .failed {
            touchPoint = nil
        } else if point.x >= 0, point.x < bottomScreen.bounds.width,
                  point.y >= 0, point.y < bottomScreen.bounds.height {
            touchPoint = CGPoint(x: point.x / bottomScreen.bounds.width * 320,
                                 y: point.y / bottomScreen.bounds.height * 240)
        }
    }

    private func configureAudio() {
        audioEngine.attach(audioPlayer)
        audioEngine.connect(audioPlayer, to: audioEngine.mainMixerNode, format: audioFormat)
        do {
            try AVAudioSession.sharedInstance().setCategory(.playback, mode: .default)
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

    @objc private func advanceFrame() {
        guard !frameInFlight else { return }
        frameInFlight = true
        let buttonMask = controlDefinitions.reduce(UInt32(0)) { result, item in
            buttonsDown.contains(item.key) ? result | item.bit : result
        }
        let inputX = circleX
        let inputY = circleY
        let point = touchPoint
        runtimeQueue.async { [weak self] in
            guard let self else { return }
            _ = YKW2ImportBridge.setZakuroButtons(
                buttonMask, circleX: inputX, circleY: inputY,
                touchX: point.map { Int($0.x) } ?? -1,
                touchY: point.map { Int($0.y) } ?? -1
            )
            let outcome = YKW2ImportBridge.runZakuroFrames(1)
            let topData = outcome == 1 ? YKW2ImportBridge.zakuroTopScreenRGBA() : Data()
            let bottomData = outcome == 1 ? YKW2ImportBridge.zakuroBottomScreenRGBA() : Data()
            let audioData = outcome == 1 ? YKW2ImportBridge.takeZakuroAudioFrames(2048) : Data()
            let topImage = outcome == 1 ? self.image(fromRGBA: topData, width: 400, height: 240) : nil
            let bottomImage = outcome == 1 ? self.image(fromRGBA: bottomData, width: 320, height: 240) : nil
            if !audioData.isEmpty { self.audioQueue.async { self.playAudio(audioData) } }
            DispatchQueue.main.async {
                self.frameInFlight = false
                guard self.isViewLoaded, self.view.window != nil else { return }
                guard outcome == 1 else {
                    self.displayLink?.invalidate()
                    self.statusLabel.text = "Zakuro detuvo la emulación (estado \(outcome))."
                    return
                }
                self.topScreen.image = topImage
                self.bottomScreen.image = bottomImage
            }
        }
    }

    private func image(fromRGBA data: Data, width: Int, height: Int) -> UIImage? {
        guard data.count == width * height * 4,
              let provider = CGDataProvider(data: data as CFData),
              let cgImage = CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32,
                                    bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(),
                                    bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue).union(.byteOrder32Big),
                                    provider: provider, decode: nil, shouldInterpolate: false, intent: .defaultIntent) else { return nil }
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

    @objc private func controlPressed(_ sender: UIButton) {
        guard let key = sender.accessibilityLabel else { return }
        if editorMode {
            selectControl(key)
        } else {
            buttonsDown.insert(key)
        }
    }

    @objc private func controlReleased(_ sender: UIButton) {
        guard !editorMode, let key = sender.accessibilityLabel else { return }
        buttonsDown.remove(key)
    }

    @objc private func dragControl(_ recognizer: UIPanGestureRecognizer) {
        guard editorMode, let control = recognizer.view else { return }
        let key = (control as? UIButton)?.accessibilityLabel ?? "STICK"
        let translation = recognizer.translation(in: controlsOverlay)
        let start = controlPositions[key] ?? CGPoint(x: 0.5, y: 0.5)
        controlPositions[key] = CGPoint(
            x: min(0.98, max(0.02, start.x + translation.x / controlsOverlay.bounds.width)),
            y: min(0.98, max(0.02, start.y + translation.y / controlsOverlay.bounds.height))
        )
        recognizer.setTranslation(.zero, in: controlsOverlay)
        layoutTouchControls()
        if recognizer.state == .ended || recognizer.state == .cancelled { saveControlLayout() }
    }

    @objc private func goBack() {
        navigationController?.popViewController(animated: true)
    }

    @objc private func toggleEditor() {
        editorMode.toggle()
        editorPanel.isHidden = !editorMode
        controlsOverlay.subviews.compactMap { $0 as? AnalogStickControl }.forEach { $0.isEditingControls = editorMode }
        controlsOverlay.subviews.compactMap { $0 as? UIButton }.forEach { $0.alpha = editorMode ? 0.88 : 1 }
        selectedControlButton.isHidden = !editorMode
        updateEditorSelectionUI()
        if !editorMode { saveControlLayout() }
    }

    @objc private func chooseControl() {
        let alert = UIAlertController(title: "Cambiar tamaño", message: "Selecciona un control", preferredStyle: .actionSheet)
        var labels = ["ALL": "Todos", "UP": "Cruceta · arriba", "DOWN": "Cruceta · abajo",
                      "LEFT": "Cruceta · izquierda", "RIGHT": "Cruceta · derecha", "STICK": "Joystick"]
        for definition in controlDefinitions where !["UP", "DOWN", "LEFT", "RIGHT"].contains(definition.key) {
            labels[definition.key] = definition.key == "SELECT" ? "Select" : definition.title
        }
        for key in ["ALL", "UP", "DOWN", "LEFT", "RIGHT", "STICK", "A", "B", "X", "Y", "SELECT", "START", "L", "R"] {
            let title = labels[key] ?? key
            alert.addAction(UIAlertAction(title: title, style: .default) { [weak self] _ in self?.selectControl(key) })
        }
        alert.addAction(UIAlertAction(title: "Cancelar", style: .cancel))
        if let popover = alert.popoverPresentationController {
            popover.sourceView = selectedControlButton
            popover.sourceRect = selectedControlButton.bounds
        }
        present(alert, animated: true)
    }

    private func selectControl(_ key: String) {
        selectedControl = key
        updateEditorSelectionUI()
    }

    private func updateEditorSelectionUI() {
        let title = selectedControl == "ALL" ? "Todos" : selectedControl == "STICK" ? "Joystick" : selectedControl
        selectedControlButton.setTitle("Tamaño · \(title) ▾", for: .normal)
        let current = selectedControl == "ALL" ? globalControlScale : controlSizes[selectedControl] ?? 1
        sizeSlider.value = Float(current)
        selectionText.text = "\(title) · \(Int(current * 100))%"
    }

    @objc private func changeControlSize() {
        let value = CGFloat(sizeSlider.value)
        if selectedControl == "ALL" { globalControlScale = value }
        else { controlSizes[selectedControl] = value }
        updateEditorSelectionUI()
        layoutTouchControls()
        saveControlLayout()
    }

    @objc private func resetControls() {
        let prefix = "ykw2.controls.\(layoutProfile)."
        for key in UserDefaults.standard.dictionaryRepresentation().keys where key.hasPrefix(prefix) {
            UserDefaults.standard.removeObject(forKey: key)
        }
        loadControlLayout()
        selectedControl = "ALL"
        updateEditorSelectionUI()
        layoutTouchControls()
    }
}

extension ZakuroViewController: UIGestureRecognizerDelegate {
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer,
                           shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool { true }
}

private final class AnalogStickControl: UIView {
    var onAxisChanged: ((Float, Float) -> Void)?
    var isEditingControls = false
    private var displacement = CGPoint.zero

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = false
        let pan = UIPanGestureRecognizer(target: self, action: #selector(moveStick(_:)))
        addGestureRecognizer(pan)
    }

    required init?(coder: NSCoder) { fatalError("AnalogStickControl does not use storyboards") }

    override func draw(_ rect: CGRect) {
        guard let context = UIGraphicsGetCurrentContext() else { return }
        let radius = min(bounds.width, bounds.height) * 0.48
        let center = CGPoint(x: bounds.midX, y: bounds.midY)
        context.setFillColor(UIColor(red: 0.09, green: 0.11, blue: 0.14, alpha: 0.70).cgColor)
        context.setStrokeColor(UIColor(red: 0.46, green: 0.55, blue: 0.63, alpha: 0.86).cgColor)
        context.setLineWidth(max(2, radius * 0.045))
        context.addEllipse(in: CGRect(x: center.x - radius, y: center.y - radius, width: radius * 2, height: radius * 2))
        context.drawPath(using: .fillStroke)
        let knob = radius * 0.52
        let knobCenter = CGPoint(x: center.x + displacement.x, y: center.y + displacement.y)
        context.setFillColor(UIColor(red: 0.34, green: 0.40, blue: 0.47, alpha: 0.94).cgColor)
        context.setStrokeColor(UIColor(red: 0.76, green: 0.82, blue: 0.87, alpha: 0.8).cgColor)
        context.addEllipse(in: CGRect(x: knobCenter.x - knob, y: knobCenter.y - knob, width: knob * 2, height: knob * 2))
        context.drawPath(using: .fillStroke)
    }

    @objc private func moveStick(_ gesture: UIPanGestureRecognizer) {
        guard !isEditingControls else { return }
        let radius = min(bounds.width, bounds.height) * 0.48
        let maxTravel = radius * 0.78
        switch gesture.state {
        case .began, .changed:
            let point = gesture.location(in: self)
            var dx = point.x - bounds.midX
            var dy = point.y - bounds.midY
            let length = max(1, hypot(dx, dy))
            if length > maxTravel {
                dx *= maxTravel / length
                dy *= maxTravel / length
            }
            displacement = CGPoint(x: dx, y: dy)
            let nx = Float(dx / maxTravel)
            let ny = Float(dy / maxTravel)
            // 3DS circle-pad positive Y points upward; UIKit's coordinates point down.
            onAxisChanged?(nx, -ny)
            setNeedsDisplay()
        default:
            displacement = .zero
            onAxisChanged?(0, 0)
            setNeedsDisplay()
        }
    }
}

private final class PassthroughControlsView: UIView {
    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        subviews.contains { subview in
            guard !subview.isHidden, subview.alpha > 0.01, subview.isUserInteractionEnabled else { return false }
            return subview.point(inside: subview.convert(point, from: self), with: event)
        }
    }
}
