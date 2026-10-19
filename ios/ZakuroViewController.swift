import AVFoundation
import UIKit

/// Full-screen iOS host for Zakuro, with a configurable on-screen 3DS gamepad.
final class ZakuroViewController: UIViewController {
    private let romURL: URL?
    private let dataURL: URL?
    private let configurationOnly: Bool
    private let statusLabel = UILabel()
    private let fpsLabel = UILabel()
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
    private var fpsWindowStart = CACurrentMediaTime()
    private var fpsWindowFrames = 0
    private var emulatedFrameNumber = 0
    private var buttonsDown = Set<String>()
    private var dpadButtons: UInt32 = 0
    private var circleX: Float = 0
    private var circleY: Float = 0
    private var touchPoint: CGPoint?
    private var controlButtons: [String: UIButton] = [:]
    private var dpadControl: DPadControl?
    private var editDragGestures: [UIPanGestureRecognizer] = []
    private var selectedControl = "ALL"
    private var editorMode = false
    private var layoutProfile = ""
    private var portraitScreenConstraints: [NSLayoutConstraint] = []
    private var landscapeScreenConstraints: [NSLayoutConstraint] = []
    private var portraitEditorTopConstraint: NSLayoutConstraint?
    private var landscapeEditorTopConstraint: NSLayoutConstraint?
    private var controlPositions: [String: CGPoint] = [:]
    private var controlSizes: [String: CGFloat] = [:]
    private var globalControlScale: CGFloat = 1
    private let audioEngine = AVAudioEngine()
    private let audioPlayer = AVAudioPlayerNode()
    private lazy var audioFormat = AVAudioFormat(standardFormatWithSampleRate: 32_728, channels: 2)!
    private static let screenColorSpace = CGColorSpaceCreateDeviceRGB()

    private let controlDefinitions: [(key: String, title: String, bit: UInt32)] = [
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
            dpadControl?.isEditingControls = true
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
            let isInitialProfile = layoutProfile.isEmpty
            layoutProfile = nextProfile
            screenStack.axis = nextProfile == "portrait" ? .vertical : .horizontal
            NSLayoutConstraint.deactivate(portraitScreenConstraints + landscapeScreenConstraints)
            NSLayoutConstraint.activate(nextProfile == "portrait" ? portraitScreenConstraints : landscapeScreenConstraints)
            portraitEditorTopConstraint?.isActive = nextProfile == "portrait"
            landscapeEditorTopConstraint?.isActive = nextProfile == "landscape"
            if isInitialProfile || !editorMode {
                loadControlLayout()
            }
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
        controlsOverlay.clipsToBounds = false
        controlsOverlay.layer.masksToBounds = false
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

        let dpad = DPadControl()
        dpad.accessibilityLabel = "DPAD"
        dpad.onButtonsChanged = { [weak self] mask in self?.dpadButtons = mask }
        dpad.onSelected = { [weak self] in self?.selectControl("DPAD") }
        let dpadDrag = UIPanGestureRecognizer(target: self, action: #selector(dragControl(_:)))
        dpadDrag.delegate = self
        dpadDrag.cancelsTouchesInView = false
        dpadDrag.isEnabled = false
        dpad.addGestureRecognizer(dpadDrag)
        editDragGestures.append(dpadDrag)
        controlsOverlay.addSubview(dpad)
        dpadControl = dpad

        let joystick = AnalogStickControl()
        joystick.accessibilityLabel = "STICK"
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
        edit.accessibilityIdentifier = "EDITOR"
        edit.isHidden = !configurationOnly
        controlsOverlay.addSubview(edit)

        statusLabel.translatesAutoresizingMaskIntoConstraints = false
        controlsOverlay.addSubview(statusLabel)
        fpsLabel.text = "FPS --"
        fpsLabel.textColor = .white
        fpsLabel.font = .monospacedDigitSystemFont(ofSize: 12, weight: .semibold)
        fpsLabel.textAlignment = .center
        fpsLabel.backgroundColor = UIColor.black.withAlphaComponent(0.68)
        fpsLabel.layer.cornerRadius = 8
        fpsLabel.clipsToBounds = true
        fpsLabel.isUserInteractionEnabled = false
        fpsLabel.isHidden = configurationOnly
        fpsLabel.translatesAutoresizingMaskIntoConstraints = false
        controlsOverlay.addSubview(fpsLabel)
        NSLayoutConstraint.activate([
            statusLabel.centerXAnchor.constraint(equalTo: controlsOverlay.centerXAnchor),
            statusLabel.topAnchor.constraint(equalTo: controlsOverlay.safeAreaLayoutGuide.topAnchor, constant: 4),
            statusLabel.widthAnchor.constraint(lessThanOrEqualTo: controlsOverlay.widthAnchor, multiplier: 0.72),
            fpsLabel.centerXAnchor.constraint(equalTo: controlsOverlay.centerXAnchor),
            fpsLabel.topAnchor.constraint(equalTo: controlsOverlay.safeAreaLayoutGuide.topAnchor, constant: 10),
            fpsLabel.widthAnchor.constraint(greaterThanOrEqualToConstant: 58),
            fpsLabel.heightAnchor.constraint(equalToConstant: 24),
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
        instructions.text = "Elige control, ajusta tamaño y arrastra para mover"
        instructions.textColor = .white
        instructions.font = .systemFont(ofSize: 11, weight: .medium)
        instructions.textAlignment = .center
        instructions.numberOfLines = 2
        instructions.adjustsFontSizeToFitWidth = true

        selectedControlButton.setTitle("Tamaño · Todos ▾", for: .normal)
        selectedControlButton.setTitleColor(.white, for: .normal)
        selectedControlButton.titleLabel?.font = .systemFont(ofSize: 14, weight: .semibold)
        selectedControlButton.addTarget(self, action: #selector(chooseControl), for: .touchUpInside)

        selectionText.text = "Todos · 100%"
        selectionText.textColor = UIColor(white: 0.85, alpha: 1)
        selectionText.textAlignment = .center
        selectionText.font = .systemFont(ofSize: 10)

        sizeSlider.minimumValue = 0.5
        sizeSlider.maximumValue = 2
        sizeSlider.value = 1
        sizeSlider.addTarget(self, action: #selector(changeControlSize), for: .valueChanged)

        let reset = UIButton(type: .system)
        reset.setTitle("Restablecer", for: .normal)
        reset.addTarget(self, action: #selector(resetControls), for: .touchUpInside)
        let done = UIButton(type: .system)
        done.setTitle("Listo", for: .normal)
        done.addTarget(self, action: #selector(commitEditor), for: .touchUpInside)
        let hide = UIButton(type: .system)
        hide.setTitle("Ocultar", for: .normal)
        hide.addTarget(self, action: #selector(hideEditorPanel), for: .touchUpInside)
        let actions = UIStackView(arrangedSubviews: [reset, hide, done])
        actions.axis = .horizontal
        actions.distribution = .fillEqually
        actions.spacing = 4

        let content = UIStackView(arrangedSubviews: [instructions, selectedControlButton, selectionText, sizeSlider, actions])
        content.axis = .vertical
        content.spacing = 2
        content.translatesAutoresizingMaskIntoConstraints = false
        editorPanel.contentView.addSubview(content)
        let panelWidth = editorPanel.widthAnchor.constraint(equalTo: controlsOverlay.widthAnchor, multiplier: 0.82)
        panelWidth.priority = .defaultHigh
        NSLayoutConstraint.activate([
            content.leadingAnchor.constraint(equalTo: editorPanel.contentView.leadingAnchor, constant: 10),
            content.trailingAnchor.constraint(equalTo: editorPanel.contentView.trailingAnchor, constant: -10),
            content.topAnchor.constraint(equalTo: editorPanel.contentView.topAnchor, constant: 6),
            content.bottomAnchor.constraint(equalTo: editorPanel.contentView.bottomAnchor, constant: -6),
            instructions.heightAnchor.constraint(equalToConstant: 28),
            selectedControlButton.heightAnchor.constraint(equalToConstant: 28),
            selectionText.heightAnchor.constraint(equalToConstant: 14),
            sizeSlider.heightAnchor.constraint(equalToConstant: 24),
            actions.heightAnchor.constraint(equalToConstant: 30),
            editorPanel.centerXAnchor.constraint(equalTo: controlsOverlay.centerXAnchor),
            // Keep the editor below the L/R row and above the main control groups.
            panelWidth,
            editorPanel.widthAnchor.constraint(lessThanOrEqualToConstant: 360),
        ])
        portraitEditorTopConstraint = editorPanel.topAnchor.constraint(
            equalTo: controlsOverlay.safeAreaLayoutGuide.topAnchor, constant: 128
        )
        landscapeEditorTopConstraint = editorPanel.topAnchor.constraint(
            equalTo: controlsOverlay.safeAreaLayoutGuide.topAnchor, constant: 112
        )
        portraitEditorTopConstraint?.isActive = true
        landscapeEditorTopConstraint?.isActive = false
        editorPanel.isHidden = true
    }

    private func loadControlLayout() {
        guard !layoutProfile.isEmpty else { return }
        let defaults = defaultControlPositions()
        let preferences = UserDefaults.standard
        let prefix = "ykw2.controls.v7.\(layoutProfile)."
        let olderPrefixes = (2...6).reversed().map { "ykw2.controls.v\($0).\(layoutProfile)." }
        let sizePrefixes = [prefix] + olderPrefixes
        controlPositions = defaults
        for key in defaults.keys {
            guard let storedX = preferences.object(forKey: prefix + key + ".x") as? NSNumber,
                  let storedY = preferences.object(forKey: prefix + key + ".y") as? NSNumber else { continue }
            let x = storedX.doubleValue
            let y = storedY.doubleValue
            guard x.isFinite, y.isFinite else { continue }
            controlPositions[key] = CGPoint(x: min(1, max(0, x)), y: min(1, max(0, y)))
        }
        controlSizes = Dictionary(uniqueKeysWithValues: defaults.keys.map { key in
            let storedSize = sizePrefixes.compactMap {
                preferences.object(forKey: $0 + key + ".size") as? Double
            }.first
            let legacyDpadSizes = ["UP", "DOWN", "LEFT", "RIGHT"].compactMap { direction in
                sizePrefixes.dropFirst().compactMap {
                    preferences.object(forKey: $0 + direction + ".size") as? Double
                }.first
            }
            let migratedDpadSize = legacyDpadSizes.isEmpty ? 1 :
                legacyDpadSizes.reduce(0, +) / Double(legacyDpadSizes.count)
            return (key, CGFloat(storedSize ?? (key == "DPAD" ? migratedDpadSize : 1)))
        })
        let storedGlobalSize = sizePrefixes.compactMap {
            preferences.object(forKey: $0 + "globalSize") as? Double
        }.first
        globalControlScale = CGFloat(storedGlobalSize ?? 1)
        setNeedsStatusBarAppearanceUpdate()
    }

    private func defaultControlPositions() -> [String: CGPoint] {
        let safeWidth = max(1, controlsOverlay.bounds.width - 16)
        let safeHeight = max(1, controlsOverlay.bounds.height
            - controlsOverlay.safeAreaInsets.top - controlsOverlay.safeAreaInsets.bottom - 16)
        let referenceSize = min(72, max(46, min(safeWidth * 0.15, safeHeight * 0.095)))
        let dpadMarginX = (referenceSize * 1.4 + 8) / safeWidth
        let faceStepX = referenceSize * 0.76 / safeWidth
        let faceStepY = referenceSize * 0.82 / safeHeight
        let faceMarginX = (referenceSize * 1.55 + 8) / safeWidth
        let faceCenter = CGPoint(x: min(0.82, 1 - faceMarginX), y: 0.57)
        let dpadCenterY: CGFloat = layoutProfile == "landscape" ? 0.55 : 0.59
        let stickCenterY: CGFloat = layoutProfile == "landscape" ? 0.86 : 0.83
        let utilityCenterY: CGFloat = layoutProfile == "landscape" ? 0.92 : 0.89
        return [
            "DPAD": CGPoint(x: max(0.18, dpadMarginX), y: dpadCenterY),
            "STICK": CGPoint(x: 0.19, y: stickCenterY),
            "X": CGPoint(x: faceCenter.x, y: faceCenter.y - faceStepY),
            "Y": CGPoint(x: faceCenter.x - faceStepX, y: faceCenter.y),
            "A": CGPoint(x: faceCenter.x + faceStepX, y: faceCenter.y),
            "B": CGPoint(x: faceCenter.x, y: faceCenter.y + faceStepY),
            "SELECT": CGPoint(x: 0.66, y: utilityCenterY), "START": CGPoint(x: 0.88, y: utilityCenterY),
            "L": CGPoint(x: 0.09, y: layoutProfile == "landscape" ? 0.20 : 0.10),
            "R": CGPoint(x: 0.91, y: layoutProfile == "landscape" ? 0.20 : 0.10),
        ]
    }

    private func saveControlLayout(forceSynchronize: Bool = false) {
        guard !controlPositions.isEmpty else { return }
        let preferences = UserDefaults.standard
        // Save placement only for the active orientation; syncing coordinates
        // across portrait and landscape made the controls jump and overlap.
        for profile in ["portrait", "landscape"] {
            let prefix = "ykw2.controls.v7.\(profile)."
            for (key, point) in controlPositions {
                preferences.set(Double(controlSizes[key] ?? 1), forKey: prefix + key + ".size")
                if profile == layoutProfile {
                    preferences.set(Double(point.x), forKey: prefix + key + ".x")
                    preferences.set(Double(point.y), forKey: prefix + key + ".y")
                }
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
        if let dpad = dpadControl {
            let unit = base * (controlSizes["DPAD"] ?? 1)
            let side = unit * 3.1
            let point = controlPositions["DPAD"] ?? CGPoint(x: 0.2, y: 0.6)
            dpad.controlUnit = unit
            dpad.bounds = CGRect(x: 0, y: 0, width: side, height: side)
            let marginX = (side / 2 + 4) / width
            let marginY = (side / 2 + 4) / height
            let safePoint = CGPoint(
                x: min(1 - marginX, max(marginX, point.x)),
                y: min(1 - marginY, max(marginY, point.y))
            )
            controlPositions["DPAD"] = safePoint
            dpad.center = CGPoint(
                x: safeFrame.minX + width * safePoint.x,
                y: safeFrame.minY + height * safePoint.y
            )
            dpad.setNeedsDisplay()
        }
        if let stick = controlsOverlay.subviews.compactMap({ $0 as? AnalogStickControl }).first {
            let p = controlPositions["STICK"] ?? CGPoint(x: 0.15, y: 0.84)
            let diameter = min(min(width * 0.48, height * 0.28), base * 2.5 * (controlSizes["STICK"] ?? 1))
            let drawingDiameter = diameter * 1.3
            stick.controlDiameter = diameter
            stick.bounds = CGRect(x: 0, y: 0, width: drawingDiameter, height: drawingDiameter)
            let stickPosition = CGPoint(
                x: min(1, max(0, p.x)),
                y: min(1, max(0, p.y))
            )
            let normalizedMarginX = (diameter / 2 + 4) / width
            let normalizedMarginY = (diameter / 2 + 4) / height
            let safeStickPosition = CGPoint(
                x: min(1 - normalizedMarginX, max(normalizedMarginX, stickPosition.x)),
                y: min(1 - normalizedMarginY, max(normalizedMarginY, stickPosition.y))
            )
            controlPositions["STICK"] = safeStickPosition
            stick.center = CGPoint(
                x: safeFrame.minX + width * safeStickPosition.x,
                y: safeFrame.minY + height * safeStickPosition.y
            )
            stick.setNeedsDisplay()
        }
        for button in controlsOverlay.subviews.compactMap({ $0 as? UIButton }) {
            switch button.accessibilityIdentifier {
            case "BACK":
                button.frame = CGRect(x: 12, y: controlsOverlay.safeAreaInsets.top + 8, width: 42, height: 36)
            case "EDITOR":
                button.isHidden = !configurationOnly
                button.frame = CGRect(
                    x: controlsOverlay.bounds.width - 54,
                    y: controlsOverlay.safeAreaInsets.top + 8,
                    width: 42,
                    height: 36
                )
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
        fpsWindowStart = CACurrentMediaTime()
        fpsWindowFrames = 0
        fpsLabel.text = "FPS --"
        fpsLabel.isHidden = false
        displayLink?.invalidate()
        displayLink = CADisplayLink(target: self, selector: #selector(advanceFrame))
        // Present one emulated frame per display refresh; game timing stays
        // native because the bundled FPS patches are opt-in.
        displayLink?.preferredFramesPerSecond = 60
        displayLink?.add(to: .main, forMode: .common)
    }

    @objc private func advanceFrame() {
        guard !frameInFlight else { return }
        frameInFlight = true
        let buttonMask = controlDefinitions.reduce(UInt32(0)) { result, item in
            buttonsDown.contains(item.key) ? result | item.bit : result
        } | dpadButtons
        let inputX = circleX
        let inputY = circleY
        let point = touchPoint
        runtimeQueue.async { [weak self] in
            guard let self else { return }
            let captureScreens = self.emulatedFrameNumber.isMultiple(of: 2)
            self.emulatedFrameNumber &+= 1
            let frameResult = autoreleasepool { () -> (Int, UIImage?, UIImage?, Data) in
                _ = YKW2ImportBridge.setZakuroButtons(
                    buttonMask, circleX: inputX, circleY: inputY,
                    touchX: point.map { Int($0.x) } ?? -1,
                    touchY: point.map { Int($0.y) } ?? -1
                )
                let runOutcome = YKW2ImportBridge.runZakuroFrames(1)
                let topData = captureScreens ? NSMutableData(length: 400 * 240 * 4)! : nil
                let bottomData = captureScreens ? NSMutableData(length: 320 * 240 * 4)! : nil
                let copyOutcome: Int
                if runOutcome != 1 {
                    copyOutcome = -1
                } else if let topData, let bottomData {
                    copyOutcome = YKW2ImportBridge.copyZakuroScreens(toTop: topData, bottom: bottomData)
                } else {
                    copyOutcome = 0
                }
                let outcome = runOutcome == 1 && copyOutcome != 0 ? -2 : runOutcome
                let audioData = outcome == 1 ? YKW2ImportBridge.takeZakuroAudioFrames(2048) : Data()
                let topImage = outcome == 1 ? topData.flatMap { self.image(fromRGBA: $0 as Data, width: 400, height: 240) } : nil
                let bottomImage = outcome == 1 ? bottomData.flatMap { self.image(fromRGBA: $0 as Data, width: 320, height: 240) } : nil
                return (outcome, topImage, bottomImage, audioData)
            }
            let (outcome, topImage, bottomImage, audioData) = frameResult
            if !audioData.isEmpty { self.enqueueAudio(audioData) }
            DispatchQueue.main.async {
                self.frameInFlight = false
                guard self.isViewLoaded, self.view.window != nil else { return }
                guard outcome == 1 else {
                    self.displayLink?.invalidate()
                    self.statusLabel.text = outcome == -3
                        ? "Zakuro encontró un error interno al ejecutar la ROM. Revisa que sea una copia descifrada compatible."
                        : "Zakuro detuvo la emulación (estado \(outcome))."
                    return
                }
                if let topImage, let bottomImage {
                    self.topScreen.image = topImage
                    self.bottomScreen.image = bottomImage
                    self.recordPresentedFrame()
                }
            }
        }
    }

    private func image(fromRGBA data: Data, width: Int, height: Int) -> UIImage? {
        guard data.count == width * height * 4,
              let provider = CGDataProvider(data: data as CFData),
              let cgImage = CGImage(width: width, height: height, bitsPerComponent: 8, bitsPerPixel: 32,
                                    bytesPerRow: width * 4, space: Self.screenColorSpace,
                                    bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue).union(.byteOrder32Big),
                                    provider: provider, decode: nil, shouldInterpolate: false, intent: .defaultIntent) else { return nil }
        return UIImage(cgImage: cgImage)
    }

    private func recordPresentedFrame() {
        let now = CACurrentMediaTime()
        fpsWindowFrames += 1
        let elapsed = now - fpsWindowStart
        guard elapsed >= 1 else { return }
        fpsLabel.text = "FPS \(Int((Double(fpsWindowFrames) / elapsed).rounded()))"
        fpsWindowFrames = 0
        fpsWindowStart = now
    }

    private func enqueueAudio(_ data: Data) {
        audioBufferLock.lock()
        // Keep a short reserve for frames that take longer on mobile CPUs.
        // Dropping an entire per-frame PCM block is heard as a sharp audio gap.
        guard pendingAudioBuffers < 12 else {
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
        let key = control.accessibilityLabel ?? "STICK"
        let translation = recognizer.translation(in: controlsOverlay)
        let safeFrame = controlsOverlay.bounds.inset(by: UIEdgeInsets(
            top: controlsOverlay.safeAreaInsets.top + 8, left: 8,
            bottom: controlsOverlay.safeAreaInsets.bottom + 8, right: 8
        ))
        let start = controlPositions[key] ?? CGPoint(x: 0.5, y: 0.5)
        let minX = key == "STICK" || key == "DPAD" ? control.bounds.width / 2 / safeFrame.width : 0
        let minY = key == "STICK" || key == "DPAD" ? control.bounds.height / 2 / safeFrame.height : 0
        controlPositions[key] = CGPoint(
            x: min(1 - minX, max(minX, start.x + translation.x / safeFrame.width)),
            y: min(1 - minY, max(minY, start.y + translation.y / safeFrame.height))
        )
        recognizer.setTranslation(.zero, in: controlsOverlay)
        layoutTouchControls()
    }

    @objc private func goBack() {
        navigationController?.popViewController(animated: true)
    }

    @objc private func toggleEditor() {
        guard configurationOnly else { return }
        editorPanel.isHidden.toggle()
    }

    @objc private func hideEditorPanel() {
        guard editorMode else { return }
        editorPanel.isHidden = true
    }

    @objc private func commitEditor() {
        guard editorMode else { return }
        saveControlLayout(forceSynchronize: true)
        if configurationOnly {
            navigationController?.popViewController(animated: true)
        } else {
            setEditorMode(false)
        }
    }

    private func setEditorMode(_ enabled: Bool) {
        editorMode = enabled
        dpadButtons = 0
        editDragGestures.forEach { $0.isEnabled = editorMode }
        editorPanel.isHidden = !editorMode
        controlsOverlay.subviews.compactMap { $0 as? AnalogStickControl }.forEach { $0.isEditingControls = editorMode }
        dpadControl?.isEditingControls = editorMode
        controlsOverlay.subviews.compactMap { $0 as? UIButton }.forEach { $0.alpha = editorMode ? 0.88 : 1 }
        selectedControlButton.isHidden = !editorMode
        updateEditorSelectionUI()
    }

    @objc private func chooseControl() {
        let alert = UIAlertController(title: "Cambiar tamaño", message: "Selecciona un control", preferredStyle: .actionSheet)
        var labels = ["ALL": "Todos", "DPAD": "Cruceta", "STICK": "Joystick"]
        for definition in controlDefinitions {
            labels[definition.key] = definition.key == "SELECT" ? "Select" : definition.title
        }
        for key in ["ALL", "DPAD", "STICK", "A", "B", "X", "Y", "SELECT", "START", "L", "R"] {
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
        let title = selectedControl == "ALL" ? "Todos" :
            selectedControl == "STICK" ? "Joystick" : selectedControl == "DPAD" ? "Cruceta" : selectedControl
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
    }

    @objc private func resetControls() {
        controlPositions = defaultControlPositions()
        controlSizes = Dictionary(uniqueKeysWithValues: controlPositions.keys.map { ($0, 1) })
        globalControlScale = 1
        selectedControl = "ALL"
        updateEditorSelectionUI()
        layoutTouchControls()
    }
}

extension ZakuroViewController: UIGestureRecognizerDelegate {
    func gestureRecognizer(_ gestureRecognizer: UIGestureRecognizer,
                           shouldRecognizeSimultaneouslyWith otherGestureRecognizer: UIGestureRecognizer) -> Bool { true }
}

private final class DPadControl: UIView {
    var onButtonsChanged: ((UInt32) -> Void)?
    var onSelected: (() -> Void)?
    var controlUnit: CGFloat = 0
    var isEditingControls = false {
        didSet {
            activeTouch = nil
            onButtonsChanged?(0)
            setNeedsDisplay()
        }
    }

    private var activeMask: UInt32 = 0
    private weak var activeTouch: UITouch?

    override init(frame: CGRect) {
        super.init(frame: frame)
        isOpaque = false
        backgroundColor = .clear
        clipsToBounds = false
        isMultipleTouchEnabled = false
    }

    required init?(coder: NSCoder) { fatalError("DPadControl does not use storyboards") }

    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        if isEditingControls { return bounds.contains(point) }
        let cell = min(bounds.width, bounds.height) / 3.1
        let dx = abs(point.x - bounds.midX)
        let dy = abs(point.y - bounds.midY)
        let halfWidth = cell * 0.48
        let reach = cell * 1.5
        return (dx <= halfWidth && dy <= reach) || (dy <= halfWidth && dx <= reach)
    }

    override func draw(_ rect: CGRect) {
        guard let context = UIGraphicsGetCurrentContext() else { return }
        let cell = min(bounds.width, bounds.height) / 3.1
        let halfBar = cell * 0.48
        let armReach = cell * 1.5
        let center = CGPoint(x: bounds.midX, y: bounds.midY)
        let vertical = CGRect(x: center.x - halfBar, y: center.y - armReach,
                              width: halfBar * 2, height: armReach * 2)
        let horizontal = CGRect(x: center.x - armReach, y: center.y - halfBar,
                                width: armReach * 2, height: halfBar * 2)
        let corner = cell * 0.22

        context.setFillColor(UIColor(red: 0.08, green: 0.10, blue: 0.13, alpha: 0.82).cgColor)
        context.setStrokeColor(UIColor(red: 0.48, green: 0.57, blue: 0.66, alpha: 0.9).cgColor)
        context.setLineWidth(max(2, cell * 0.075))
        context.addPath(UIBezierPath(roundedRect: vertical, cornerRadius: corner).cgPath)
        context.addPath(UIBezierPath(roundedRect: horizontal, cornerRadius: corner).cgPath)
        context.drawPath(using: .fillStroke)

        let segments: [(UInt32, CGRect, String, CGPoint)] = [
            (1 << 6, CGRect(x: center.x - halfBar * 0.72, y: center.y - armReach + cell * 0.12,
                            width: halfBar * 1.44, height: cell * 0.78), "↑",
             CGPoint(x: center.x, y: center.y - cell)),
            (1 << 7, CGRect(x: center.x - halfBar * 0.72, y: center.y + armReach - cell * 0.90,
                            width: halfBar * 1.44, height: cell * 0.78), "↓",
             CGPoint(x: center.x, y: center.y + cell)),
            (1 << 5, CGRect(x: center.x - armReach + cell * 0.12, y: center.y - halfBar * 0.72,
                            width: cell * 0.78, height: halfBar * 1.44), "←",
             CGPoint(x: center.x - cell, y: center.y)),
            (1 << 4, CGRect(x: center.x + armReach - cell * 0.90, y: center.y - halfBar * 0.72,
                            width: cell * 0.78, height: halfBar * 1.44), "→",
             CGPoint(x: center.x + cell, y: center.y)),
        ]
        for (bit, segment, arrow, labelCenter) in segments {
            if activeMask & bit != 0 {
                context.setFillColor(UIColor(red: 0.17, green: 0.54, blue: 0.72, alpha: 0.94).cgColor)
                context.addPath(UIBezierPath(roundedRect: segment, cornerRadius: corner * 0.65).cgPath)
                context.fillPath()
            }
            let font = UIFont.systemFont(ofSize: cell * 0.53, weight: .bold)
            let attributes: [NSAttributedString.Key: Any] = [
                .font: font,
                .foregroundColor: UIColor.white.withAlphaComponent(activeMask & bit != 0 ? 1 : 0.78),
            ]
            let textSize = (arrow as NSString).size(withAttributes: attributes)
            (arrow as NSString).draw(at: CGPoint(x: labelCenter.x - textSize.width / 2,
                                                 y: labelCenter.y - textSize.height / 2),
                                     withAttributes: attributes)
        }
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        if isEditingControls {
            onSelected?()
            super.touchesBegan(touches, with: event)
            return
        }
        guard activeTouch == nil, let touch = touches.first else { return }
        activeTouch = touch
        updateDirection(at: touch.location(in: self))
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = activeTouch, touches.contains(touch) else { return }
        updateDirection(at: touch.location(in: self))
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) {
        guard let touch = activeTouch, touches.contains(touch) else { return }
        activeTouch = nil
        updateMask(0)
    }

    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        activeTouch = nil
        updateMask(0)
    }

    private func updateDirection(at point: CGPoint) {
        let cell = min(bounds.width, bounds.height) / 3.1
        let dx = point.x - bounds.midX
        let dy = point.y - bounds.midY
        let limit = cell * 0.48
        var mask: UInt32 = 0
        if abs(dx) <= limit && dy < -cell * 0.20 { mask |= 1 << 6 }
        if abs(dx) <= limit && dy > cell * 0.20 { mask |= 1 << 7 }
        if abs(dy) <= limit && dx < -cell * 0.20 { mask |= 1 << 5 }
        if abs(dy) <= limit && dx > cell * 0.20 { mask |= 1 << 4 }
        updateMask(mask)
    }

    private func updateMask(_ mask: UInt32) {
        guard activeMask != mask else { return }
        activeMask = mask
        onButtonsChanged?(mask)
        setNeedsDisplay()
    }
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
    var controlDiameter: CGFloat = 0

    private var stickRadius: CGFloat {
        let diameter = controlDiameter > 0 ? controlDiameter : min(bounds.width, bounds.height) / 1.3
        return diameter * 0.48
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        // The stick is a transparent overlay. Leaving UIView's opaque default
        // enabled causes its undrawn rectangular backing to appear black over
        // the game image on iOS.
        isOpaque = false
        backgroundColor = .clear
        clipsToBounds = false
        layer.isOpaque = false
        layer.masksToBounds = false
        layer.backgroundColor = UIColor.clear.cgColor
        isMultipleTouchEnabled = false
        let pan = UIPanGestureRecognizer(target: self, action: #selector(moveStick(_:)))
        gameplayPan = pan
        addGestureRecognizer(pan)
    }

    required init?(coder: NSCoder) { fatalError("AnalogStickControl does not use storyboards") }

    override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
        if isEditingControls { return bounds.contains(point) }
        let dx = point.x - bounds.midX
        let dy = point.y - bounds.midY
        let hitRadius = stickRadius * 1.08
        return dx * dx + dy * dy <= hitRadius * hitRadius
    }

    override func draw(_ rect: CGRect) {
        guard let context = UIGraphicsGetCurrentContext() else { return }
        let radius = stickRadius
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
        let radius = stickRadius
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
