pipeline {
    agent any

    stages {
        stage('Checkout') {
            steps {
                checkout scm
            }
        }
        
        stage('Build Firmware') {
            steps {
                // Execute in PowerShell to match your local Windows environment
                powershell '''
                    # Set the IDF path and execute the export script to load tools
                    $env:IDF_PATH = "C:\\esp\\v6.0.1\\esp-idf"
                    . $env:IDF_PATH\\export.ps1
                    
                    # Build the firmware
                    idf.py build
                '''
            }
        }
    }
    
    post {
        success {
            // If the build succeeds, save the firmware binary so we can download or publish it later
            archiveArtifacts artifacts: 'build/ota_node.bin', fingerprint: true
        }
    }
}