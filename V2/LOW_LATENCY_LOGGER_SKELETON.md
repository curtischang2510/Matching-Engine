**CURTIS CHANG**

**+65 9821 9238** [curtischang2510@gmail.com](mailto:curtischang2510@gmail.com) [GitHub](mailto:https://github.com/curtischang2510) [www.linkedin.com/in/curtis-chang/](https://www.linkedin.com/in/curtis-chang/)

**work experience**

**TikTok (Singapore),** _SOFTWARE BACKEND ENGINEER INTERN_ **May 2026 - Present**

- Implement features in TikTok shop driving a 46% increase in user engagement and time-on-page
- Identified LFU-LRU cache mismatch against TikTok Shop's trend-driven access patterns, validated via parallel live traffic split, improving cache hit rate from 54% to 79% and cutting downstream database query load by 60%
- Contributed to core database migration from relational to graph model, owning online and offline validation phase eliminating complex multi-table-joins and reducing query latency by 62%
- Built PySpark offline verification pipeline comparing MySQL and graph DB edge data at scale, detecting schema and attribute mismatches daily cutting manual data reconciliation effort by ~90%

**TERRA API (London),** _SOFTWARE BACKEND ENGINEER INTERN_ **Aug 2025 - Dec 2025**

[Vantage API](https://docs.tryterra.co/vantage-api-docs) _-_ Architected microservices-based order management platform for at-home blood test kit fulfilment in Go with infrastructure-as-code deployment, processing supplier integrations and client webhook notifications

- **Built RESTful APIs** for order creation, kit activation, and results retrieval with robust input validation and centralized error-handling middleware, supporting 200+ daily orders across 3 supplier integrations
- Provisioned and deployed a Terraform-managed microservices stack on GCP, using Apache Pulsar and gRPC for inter-service communication and automated deployments to Compute Engine, achieving 99.9% uptime
- Designed fault-tolerant webhook delivery system with two-layer retry strategy using exponential backoff and Apache Pulsar dead-letter queue, reducing webhook failure rate from 15% to under 2%.
- Modelled a PostgreSQL schema using SQLC for type-safe queries, enforcing data integrity through normalization, foreign keys, and atomic transactions
- Implemented CI/CD with GitHub Actions to build and tag Docker images on each merge, and pushed artifacts to Google Artifact Registry reducing manual deployment time from 15 minutes to under 2 minutes

**DSO National Laboratories (Singapore),** _SOFTWARE ENGINEER INTERN_ **May 2025 - Aug 2025**

- Built a Python data-generation pipeline to render synthetic underwater scenes in Blender and export optical frames + metadata for sonar simulation
- Implemented optical-to-sonar translation algorithm based on published research paper using Python and OpenCV.
- Parallelised dataset generation using Python's ProcessPoolExecutor to parallelize image translation, improving throughput by 50% across CPU cores
- Trained YOLOv8 model on 10k+ synthetic sonar images, achieves 91% test detection accuracy

**education**

**National University of Singapore Aug 2023 - May 2027**

Bachelor of Computing in Computer Science

- GPA: 4.75 / 5.0 (First Class Honors)
- Focus Areas: Artificial Intelligence, Parallel Computing

**skills & abilities**

- **Languages**: Python, C, C++, Rust, Go, Java, JavaFX, JavaScript, SQL, Bash
- **Frameworks**: PyTorch, OpenCV, React.js, JUnit, gRPC
- **DevOps / Cloud / Systems**: Docker, GCP, Terraform, Pulsar, PostgreSQL, ClickhouseDB, Linux, GraphQL

**SELECTED Activities**

**Orderbook,** _Personal Project_ _(In progress)_

- Built a limit orderbook in C++ optimized for Apple Silicon profiled with Instruments
- Identified cache-miss bottlenecks, and replaced std::deque with custom circular buffer to improve data locality and eliminate dynamic allocations in the hot path

**Research Assistant,** _School of Computing_ **Jan 2025 - May 2025**

- Reproduced a research paper's autoencoder architecture in PyTorch to remove adversarial perturbations prior to vision-language model inference
- Improved attack-set accuracy from 33% to 84% under PGD attacks using autoencoder-based input sanitiser
- Achieved PSNR = 30dB and SSIM = 0.81 on sanitised vs clean images

&nbsp;

**Teaching Assistant,** _CS2040 Data Structure & Algorithms_ **Aug 2024 - May 2025**

- Taught fundamentals of data structures and algorithms to 50+ students, receiving 4.5/5 student evaluation rating