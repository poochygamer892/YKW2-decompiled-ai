import AVFoundation
import UIKit

/// Full-screen iOS host for Zakuro, with a configurable on-screen 3DS gamepad.
final class ZakuroViewController: UIViewController {
    private let romURL: URL?
    private let dataURL: URL?
    private let configurationOnly: Bool
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
    private let audioBufferLock = NSLock()
    private var pendingAudioBuffers = 0
    private var frameInFlight = false
    private var buttonsDown = Set<String>()
    private var circleX: Float = 0
    private var circleY: Float = 0
    private var touchPoint: CGPoint?
    private var controlButtons: [String: UIButton] = [:]
    private var editDragGestures: [UIPanGestureRecognizer] = []
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

    init(romURL: URL? = nil, dataURL: URL? = nil, configurationOnly: Bool = false) {
        self.romURL = romURL
        self.dataURL = dataURL
        self.configurationOnly = configurationOnly
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
        if configurationOnly && isMovingFromParent {
            saveControlLayout(forceSynchronize: true)
        }
        if isMovingFromParent || isBeingDismissed {
            displayLink?.invalidate()
            displayLink = nil
            audioPlayer.stop()
            audioEngine.stop()
            audioBufferLock.lock()
            pendingAudioBuffers = 0
            audioBufferLock.unlock()
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
        if configurationOnly {
            statusLabel.isHidden = true
            editorMode = true
            editDragGestures.forEach { $0.isEnabled = true }
            editorPanel.isHidden = false
            controlsOverlay.subviews.compactMap { $0 as? AnalogStickControl }
                .forEach { $0.isEditingControls = true }
        } else {
            configureAudio()
            statusLabel.text = "Abriendo la ROM con Zakuro…"
            startZakuro()
        }
    }

    private func startZakuro() {
        guard let romURL = self.romURL, let dataURL = self.dataURL else {
            statusLabel.text = "No se proporcionó una ROM local. Vuelve al menú e impórtala de nuevo."
            return
        }
        DispatchQueue.global(qos: .userInitiated).async { [weak self, romURL, dataURL] in
            guard let self else { return }
            let error = YKW2ImportBridge.openZakuroROM(at: romURL, dataDirectoryURL: dataURL)
            DispatchQueue.main.async {
                guard self.viewIfLoaded?.window != nil else {
                    YKW2ImportBridge.closeZakuro()
                    return
                }
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
        screenStack.alignment = .center
        screenStack.spacing = 8
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
            bottomScreen.widthAnchor.constraint(equalTo: topScreen.widthAnchor, multiplier: 0.8),
            bottomScreen.heightAnchor.constraint(equalTo: topScreen.heightAnchor),
        ]
        landscapeScreenConstraints = [
            topScreen.heightAnchor.constraint(equalTo: screenStack.heightAnchor),
            topScreen.widthAnchor.constraint(equalTo: topScreen.heightAnchor, multiplier: 5.0 / 3.0),
            bottomScreen.heightAnchor.constraint(equalTo: topScreen.heightAnchor),
            bottomScreen.widthAnchor.constraint(equalTo: topScreen.widthAnchor, multiplier: 0.8),
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
            drag.isEnabled = false
            button.addGestureRecognizer(drag)
            editDragGestures.append(drag)
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
        joystickDrag.isEnabled = false
        joystick.addGestureRecognizer(joystickDrag)
        editDragGestures.append(joystickDrag)
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
        let panelWidth = editorPanel.widthAnchor.constraint(equalTo: controlsOverlay.widthAnchor, multiplier: 0.82)
        panelWidth.priority = .defaultHigh
        NSLayoutConstraint.activate([
            content.leadingAnchor.constraint(equalTo: editorPanel.contentView.leadingAnchor, constant: 10),
            content.trailingAnchor.constraint(equalTo: editorPanel.contentView.trailingAnchor, constant: -10),
            content.topAnchor.constraint(equalTo: editorPanel.contentView.topAnchor, constant: 6),
            content.bottomAnchor.constraint(equalTo: editorPanel.contentView.bottomAnchor, constant: -6),
            editorPanel.centerXAnchor.constraint(equalTo: controlsOverlay.centerXAnchor),
            editorPanel.topAnchor.constraint(equalTo: controlsOverlay.safeAreaLayoutGuide.topAnchor, constant: 8),
            panelWidth,
            editorPanel.widthAnchor.constraint(lessThanOrEqualToConstant: 360),
        ])
        editorPanel.isHidden = true
    }

    private func loadControlLayout() {
        guard !layoutProfile.isEmpty else { return }
        let safeWidth = max(1, controlsOverlay.bounds.width - 16)
        let safeHeight = max(1, controlsOverlay.bounds.height
            - controlsOverlay.safeAreaInsets.top - controlsOverlay.safeAreaInsets.bottom - 16)
        let referenceSize = min(54, max(34, min(safeWidth * 0.15, safeHeight * 0.075)))
        let dpadStepX = referenceSize * 1.12 / safeWidth
        let dpadStepY = referenceSize * 1.12 / safeHeight
        let edgeMargin = (referenceSize / 2 + 8) / safeWidth
        let dpadCenterX = max(0.17, dpadStepX + edgeMargin)
        let faceCenter = CGPoint(x: min(0.82, 1 - dpadStepX - edgeMargin), y: 0.76)
        let defaults: [String: CGPoint] = [
            "UP": CGPoint(x: dpadCenterX, y: 0.66 - dpadStepY),
            "DOWN": CGPoint(x: dpadCenterX, y: 0.66 + dpadStepY),
            "LEFT": CGPoint(x: dpadCenterX - dpadStepX, y: 0.66),
            "RIGHT": CGPoint(x: dpadCenterX + dpadStepX, y: 0.66),
            "STICK": CGPoint(x: 0.18, y: 0.91),
            "X": CGPoint(x: faceCenter.x, y: faceCenter.y - dpadStepY),
            "Y": CGPoint(x: faceCenter.x - dpadStepX, y: faceCenter.y),
            "A": CGPoint(x: faceCenter.x + dpadStepX, y: faceCenter.y),
            "B": CGPoint(x: faceCenter.x, y: faceCenter.y + dpadStepY),
            "SELECT": CGPoint(x: 0.64, y: 0.66), "START": CGPoint(x: 0.88, y: 0.66),
            "L": CGPoint(x: 0.09, y: 0.10), "R": CGPoint(x: 0.91, y: 0.10),
        ]
        let preferences = UserDefaults.standard
        let prefix = "ykw2.controls.v4.\(layoutProfile)."
        let previousPrefix = "ykw2.controls.v3.\(layoutProfile)."
        let legacySizePrefix = "ykw2.controls.v2.\(layoutProfile)."
        controlPositions = defaults.mapValues { $0 }
        // v4 uses a fresh 3DS-aware default layout. Continue honoring saved
        // positions edited in v4, while carrying forward size preferences.
        for key in defaults.keys {
            guard let storedX = preferences.object(forKey: prefix + key + ".x") as? NSNumber,
                  let storedY = preferences.object(forKey: prefix + key + ".y") as? NSNumber else { continue }
            let x = storedX.doubleValue
            let y = storedY.doubleValue
            guard x.isFinite, y.isFinite else { continue }
            controlPositions[key] = CGPoint(x: min(1, max(0, x)), y: min(1, max(0, y)))
        }
        controlSizes = Dictionary(uniqueKeysWithValues: defaults.keys.map { key in
            let currentSize = preferences.object(forKey: prefix + key + ".size") as? Double
            let previousSize = preferences.object(forKey: previousPrefix + key + ".size") as? Double
            let legacySize = preferences.object(forKey: legacySizePrefix + key + ".size") as? Double
            return (key, CGFloat(currentSize ?? previousSize ?? legacySize ?? 1))
        })
        let currentGlobalSize = preferences.object(forKey: prefix + "globalSize") as? Double
        let previousGlobalSize = preferences.object(forKey: previousPrefix + "globalSize") as? Double
        let legacyGlobalSize = preferences.object(forKey: legacySizePrefix + "globalSize") as? Double
        globalControlScale = CGFloat(currentGlobalSize ?? previousGlobalSize ?? legacyGlobalSize ?? 1)
        setNeedsStatusBarAppearanceUpdate()
    }

    private func saveControlLayout(forceSynchronize: Bool = false) {
        guard !controlPositions.isEmpty else { return }
        let preferences = UserDefaults.standard
        // The editor can be opened in portrait while the game is later played
        // in landscape. Keep both profiles in sync so changing orientation
        // cannot make the app appear to have discarded the user's layout.
        for profile in ["portrait", "landscape"] {
            let prefix = "ykw2.controls.v4.\(profile)."
            for (key, point) in controlPositions {
                preferences.set(Double(point.x), forKey: prefix + key + ".x")
                preferences.set(Double(point.y), forKey: prefix + key + ".y")
                preferences.set(Double(controlSizes[key] ?? 1), forKey: prefix + key + ".size")
            }
            preferences.set(Double(globalControlScale), forKey: prefix + "globalSize")
        }
        preferences.set(Date().timeIntervalSince1970, forKey: "ykw2.controls.lastSavedAt")
        if forceSynchronize {
            preferences.synchronize()
        }
    }

    private func layoutTouchControls() {
        guard controlsOverlay.bounds.width > 0, controlsOverlay.bounds.height > 0 else { return }
        let safeFrame = controlsOverlay.bounds.inset(by: UIEdgeInsets(
            top: controlsOverlay.safeAreaInsets.top + 8,
            left: 8,
            bottom: controlsOverlay.safeAreaInsets.bottom + 8,
            right: 8
        ))
        guard safeFrame.width > 0, safeFrame.height > 0 else { return }
        let width = safeFrame.width
        let height = safeFrame.height
        let base = min(54, max(34, min(width * 0.15, height * 0.075))) * globalControlScale
        for definition in controlDefinitions {
            guard let button = controlButtons[definition.key] else { continue }
            let center = controlPositions[definition.key] ?? CGPoint(x: 0.85, y: 0.7)
            let size = base * (controlSizes[definition.key] ?? 1)
            let isShoulder = definition.key == "L" || definition.key == "R"
            let isUtility = definition.key == "SELECT" || definition.key == "START"
            let frameSize = isShoulder || isUtility ? CGSize(width: size * (definition.key == "START" ? 1.65 : 1.2), height: size * 0.74) : CGSize(width: size, height: size)
            button.bounds = CGRect(origin: .zero, size: frameSize)
            button.center = CGPoint(
                x: min(safeFrame.maxX - frameSize.width / 2, max(safeFrame.minX + frameSize.width / 2, safeFrame.minX + width * center.x)),
                y: min(safeFrame.maxY - frameSize.height / 2, max(safeFrame.minY + frameSize.height / 2, safeFrame.minY + height * center.y))
            )
            button.layer.cornerRadius = isShoulder || isUtility ? frameSize.height / 2 : frameSize.width / 2
            button.titleLabel?.font = .systemFont(ofSize: min(20, max(11, frameSize.height * 0.35)), weight: .bold)
        }
        if let stick = controlsOverlay.subviews.compactMap({ $0 as? AnalogStickControl }).first {
            let p = controlPositions["STICK"] ?? CGPoint(x: 0.15, y: 0.84)
            let diameter = min(min(width * 0.48, height * 0.28), base * 3.2 * (controlSizes["STICK"] ?? 1))
            stick.bounds = CGRect(x: 0, y: 0, width: diameter, height: diameter)
            stick.center = CGPoint(
                x: min(safeFrame.maxX - diameter / 2, max(safeFrame.minX + diameter / 2, safeFrame.minX + width * p.x)),
                y: min(safeFrame.maxY - diameter / 2, max(safeFrame.minY + diameter / 2, safeFrame.minY + height * p.y))
            )
            stick.setNeedsDisplay()
        }
        for button in controlsOverlay.subviews.compactMap({ $0 as? UIButton }) {
            switch button.accessibilityIdentifier {
            case "BACK":
                button.frame = CGRect(x: 12, y: controlsOverlay.safeAreaInsets.top + 8, width: 42, height: 36)
            case "EDITOR":
                button.isHidden = true
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
        // Zakuro still advances at 60 emulated frames/s. Presenting at 30 Hz
        // halves RGBA copies, CGImage/UIImage creation and main-thread updates.
        displayLink?.preferredFramesPerSecond = 30
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
            let outcome = YKW2ImportBridge.runZakuroFrames(2)
            let topData = outcome == 2 ? YKW2ImportBridge.zakuroTopScreenRGBA() : Data()
            let bottomData = outcome == 2 ? YKW2ImportBridge.zakuroBottomScreenRGBA() : Data()
            let audioData = outcome == 2 ? YKW2ImportBridge.takeZakuroAudioFrames(2048) : Data()
            let topImage = outcome == 2 ? self.image(fromRGBA: topData, width: 400, height: 240) : nil
            let bottomImage = outcome == 2 ? self.image(fromRGBA: bottomData, width: 320, height: 240) : nil
            if !audioData.isEmpty { self.enqueueAudio(audioData) }
            DispatchQueue.main.async {
                self.frameInFlight = false
                guard self.isViewLoaded, self.view.window != nil else { return }
                guard outcome == 2 else {
                    self.displayLink?.invalidate()
                    self.statusLabel.text = outcome == -3
                        ? "Zakuro encontró un error interno al ejecutar la ROM. Revisa que sea una copia descifrada compatible."
                        : "Zakuro detuvo la emulación (estado \(outcome))."
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

    private func enqueueAudio(_ data: Data) {
        audioBufferLock.lock()
        guard pendingAudioBuffers < 3 else {
            audioBufferLock.unlock()
            return
        }
        pendingAudioBuffers += 1
        audioBufferLock.unlock()
        audioQueue.async { [weak self] in self?.playAudio(data) }
    }

    private func finishAudioBuffer() {
        audioBufferLock.lock()
        pendingAudioBuffers = max(0, pendingAudioBuffers - 1)
        audioBufferLock.unlock()
    }

    private func playAudio(_ data: Data) {
        let sampleCount = data.count / MemoryLayout<Int16>.size
        let frameCount = sampleCount / 2
        guard frameCount > 0,
              let buffer = AVAudioPCMBuffer(pcmFormat: audioFormat, frameCapacity: AVAudioFrameCount(frameCount)),
              let channels = buffer.floatChannelData,
              audioEngine.isRunning else {
            finishAudioBuffer()
            return
        }
        data.withUnsafeBytes { raw in
            let samples = raw.bindMemory(to: Int16.self)
            for frame in 0..<frameCount {
                channels[0][frame] = Float(samples[frame * 2]) / Float(Int16.max)
                channels[1][frame] = Float(samples[frame * 2 + 1]) / Float(Int16.max)
            }
        }
        buffer.frameLength = AVAudioFrameCount(frameCount)
        audioPlayer.scheduleBuffer(buffer, completionCallbackType: .dataPlayedBack) { [weak self] _ in
            self?.finishAudioBuffer()
        }
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
        let safeFrame = controlsOverlay.bounds.inset(by: UIEdgeInsets(
            top: controlsOverlay.safeAreaInsets.top + 8, left: 8,
            bottom: controlsOverlay.safeAreaInsets.bottom + 8, right: 8
        ))
        let start = controlPositions[key] ?? CGPoint(x: 0.5, y: 0.5)
        controlPositions[key] = CGPoint(
            x: min(1, max(0, start.x + translation.x / safeFrame.width)),
            y: min(1, max(0, start.y + translation.y / safeFrame.height))
        )
        recognizer.setTranslation(.zero, in: controlsOverlay)
        layoutTouchControls()
        if recognizer.state == .changed {
            saveControlLayout()
        } else if recognizer.state == .ended || recognizer.state == .cancelled || recognizer.state == .failed {
            saveControlLayout(forceSynchronize: true)
        }
    }

    @objc private func goBack() {
        navigationController?.popViewController(animated: true)
    }

    @objc private func toggleEditor() {
        if configurationOnly && editorMode {
            saveControlLayout(forceSynchronize: true)
            navigationController?.popViewController(animated: true)
            return
        }
        editorMode.toggle()
        editDragGestures.forEach { $0.isEnabled = editorMode }
        editorPanel.isHidden = !editorMode
        controlsOverlay.subviews.compactMap { $0 as? AnalogStickControl }.forEach { $0.isEditingControls = editorMode }
        controlsOverlay.subviews.compactMap { $0 as? UIButton }.forEach { $0.alpha = editorMode ? 0.88 : 1 }
        selectedControlButton.isHidden = !editorMode
        updateEditorSelectionUI()
        if !editorMode { saveControlLayout(forceSynchronize: true) }
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
        let prefixes = ["portrait", "landscape"].flatMap { profile in
            ["ykw2.controls.v2.\(profile).", "ykw2.controls.v3.\(profile).", "ykw2.controls.v4.\(profile)."]
        }
        for key in UserDefaults.standard.dictionaryRepresentation().keys {
            if prefixes.contains(where: { key.hasPrefix($0) }) {
                UserDefaults.standard.removeObject(forKey: key)
            }
        }
        loadControlLayout()
        selectedControl = "ALL"
        updateEditorSelectionUI()
        layoutTouchControls()
        saveControlLayout(forceSynchronize: true)
    }
}

extension ZakuroViewController: UIGestureRecognizerDelegate {
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer,
                           shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool { true }
}

private final class AnalogStickControl: UIView {
    var onAxisChanged: ((Float, Float) -> Void)?
    var isEditingControls = false {
        didSet {
            gameplayPan?.isEnabled = !isEditingControls
            if isEditingControls {
                displacement = .zero
                onAxisChanged?(0, 0)
                setNeedsDisplay()
            }
        }
    }
    private var displacement = CGPoint.zero
    private weak var gameplayPan: UIPanGestureRecognizer?

    override init(frame: CGRect) {
        super.init(frame: frame)
        // The stick is a transparent overlay. Leaving UIView's opaque default
        // enabled causes its undrawn rectangular backing to appear black over
        // the game image on iOS.
        isOpaque = false
        backgroundColor = .clear
        layer.isOpaque = false
        layer.backgroundColor = UIColor.clear.cgColor
        isMultipleTouchEnabled = false
        let pan = UIPanGestureRecognizer(target: self, action: #selector(moveStick(_:)))
        gameplayPan = pan
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
        case .began:
            // Treat the initial finger contact as the neutral point. This avoids
            // a jump when the thumb lands off-center and still tracks outside
            // the visible ring while UIKit keeps the pan gesture alive.
            gesture.setTranslation(.zero, in: self)
            displacement = .zero
            onAxisChanged?(0, 0)
            setNeedsDisplay()
        case .changed:
            let delta = gesture.translation(in: self)
            var dx = delta.x
            var dy = delta.y
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
