package utils

import (
	"fmt"
	"sort"
	"strconv"
	"strings"

	networkingv1beta1 "k8s.io/api/networking/v1beta1"

	prometheusv1 "github.com/coreos/prometheus-operator/pkg/apis/monitoring/v1"
	"github.com/eclipse/openj9/pkg/common"

	servingv1alpha1 "github.com/knative/serving/pkg/apis/serving/v1alpha1"
	routev1 "github.com/openshift/api/route/v1"
	"github.com/operator-framework/operator-sdk/pkg/k8sutil"
	openj9v1beta1 "github.com/eclipse/openj9/pkg/apis/openj9/v1beta1"
	appsv1 "k8s.io/api/apps/v1"
	autoscalingv1 "k8s.io/api/autoscaling/v1"
	corev1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/api/equality"
	"k8s.io/apimachinery/pkg/api/resource"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/util/intstr"
)

// CustomizeDeployment ...
func CustomizeDeployment(deploy *appsv1.Deployment, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	deploy.Labels = ba.GetLabels()
	deploy.Annotations = MergeMaps(deploy.Annotations, ba.GetAnnotations())

	if ba.GetAutoscaling() == nil {
		deploy.Spec.Replicas = ba.GetReplicas()
	}

	if deploy.Spec.Selector == nil {
		deploy.Spec.Selector = &metav1.LabelSelector{
			MatchLabels: map[string]string{
				"app.kubernetes.io/instance": obj.GetName(),
			},
		}
	}

	UpdateAppDefinition(deploy.Labels, deploy.Annotations, ba)
}

// CustomizeStatefulSet ...
func CustomizeStatefulSet(statefulSet *appsv1.StatefulSet, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	statefulSet.Labels = ba.GetLabels()
	statefulSet.Annotations = MergeMaps(statefulSet.Annotations, ba.GetAnnotations())

	if ba.GetAutoscaling() == nil {
		statefulSet.Spec.Replicas = ba.GetReplicas()
	}
	statefulSet.Spec.ServiceName = obj.GetName() + "-headless"
	if statefulSet.Spec.Selector == nil {
		statefulSet.Spec.Selector = &metav1.LabelSelector{
			MatchLabels: map[string]string{
				"app.kubernetes.io/instance": obj.GetName(),
			},
		}
	}

	UpdateAppDefinition(statefulSet.Labels, statefulSet.Annotations, ba)
}

// UpdateAppDefinition adds or removes kAppNav auto-create related annotations/labels
func UpdateAppDefinition(labels map[string]string, annotations map[string]string, ba common.BaseComponent) {
	if ba.GetCreateAppDefinition() != nil && !*ba.GetCreateAppDefinition() {
		delete(labels, "kappnav.app.auto-create")
		delete(annotations, "kappnav.app.auto-create.name")
		delete(annotations, "kappnav.app.auto-create.kinds")
		delete(annotations, "kappnav.app.auto-create.label")
		delete(annotations, "kappnav.app.auto-create.labels-values")
		delete(annotations, "kappnav.app.auto-create.version")
	} else {
		labels["kappnav.app.auto-create"] = "true"
		annotations["kappnav.app.auto-create.name"] = ba.GetApplicationName()
		annotations["kappnav.app.auto-create.kinds"] = "Deployment, StatefulSet, Service, Route, Ingress, ConfigMap"
		annotations["kappnav.app.auto-create.label"] = "app.kubernetes.io/part-of"
		annotations["kappnav.app.auto-create.labels-values"] = ba.GetApplicationName()
		if ba.GetVersion() == "" {
			delete(annotations, "kappnav.app.auto-create.version")
		} else {
			annotations["kappnav.app.auto-create.version"] = ba.GetVersion()
		}
	}
}

// CustomizeRoute ...
func CustomizeRoute(route *routev1.Route, ba common.BaseComponent, key string, crt string, ca string, destCACert string) {
	obj := ba.(metav1.Object)
	route.Labels = ba.GetLabels()
	route.Annotations = MergeMaps(route.Annotations, ba.GetAnnotations())

	if ba.GetRoute() != nil {
		rt := ba.GetRoute()
		route.Annotations = MergeMaps(route.Annotations, rt.GetAnnotations())

		host := rt.GetHost()
		if host == "" && common.Config[common.OpConfigDefaultHostname] != "" {
			host = obj.GetName() + "-" + obj.GetNamespace() + "." + common.Config[common.OpConfigDefaultHostname]
		}
		route.Spec.Host = host
		route.Spec.Path = rt.GetPath()
		if ba.GetRoute().GetTermination() != nil {
			if route.Spec.TLS == nil {
				route.Spec.TLS = &routev1.TLSConfig{}
			}
			route.Spec.TLS.Termination = *rt.GetTermination()
			if route.Spec.TLS.Termination == routev1.TLSTerminationReencrypt {
				route.Spec.TLS.Certificate = crt
				route.Spec.TLS.CACertificate = ca
				route.Spec.TLS.Key = key
				route.Spec.TLS.DestinationCACertificate = destCACert
				if rt.GetInsecureEdgeTerminationPolicy() != nil {
					route.Spec.TLS.InsecureEdgeTerminationPolicy = *rt.GetInsecureEdgeTerminationPolicy()
				}
			} else if route.Spec.TLS.Termination == routev1.TLSTerminationPassthrough {
				route.Spec.TLS.Certificate = ""
				route.Spec.TLS.CACertificate = ""
				route.Spec.TLS.Key = ""
				route.Spec.TLS.DestinationCACertificate = ""
				route.Spec.TLS.InsecureEdgeTerminationPolicy = ""
			} else if route.Spec.TLS.Termination == routev1.TLSTerminationEdge {
				route.Spec.TLS.Certificate = crt
				route.Spec.TLS.CACertificate = ca
				route.Spec.TLS.Key = key
				route.Spec.TLS.DestinationCACertificate = ""
				if rt.GetInsecureEdgeTerminationPolicy() != nil {
					route.Spec.TLS.InsecureEdgeTerminationPolicy = *rt.GetInsecureEdgeTerminationPolicy()
				}
			}
		}
	}
	if ba.GetRoute() == nil || ba.GetRoute().GetTermination() == nil {
		route.Spec.TLS = nil
	}
	route.Spec.To.Kind = "Service"
	route.Spec.To.Name = obj.GetName()
	weight := int32(100)
	route.Spec.To.Weight = &weight
	if route.Spec.Port == nil {
		route.Spec.Port = &routev1.RoutePort{}
	}

	if ba.GetService().GetPortName() != "" {
		route.Spec.Port.TargetPort = intstr.FromString(ba.GetService().GetPortName())
	} else {
		route.Spec.Port.TargetPort = intstr.FromString(strconv.Itoa(int(ba.GetService().GetPort())) + "-tcp")
	}
}

// ErrorIsNoMatchesForKind ...
func ErrorIsNoMatchesForKind(err error, kind string, version string) bool {
	return strings.HasPrefix(err.Error(), fmt.Sprintf("no matches for kind \"%s\" in version \"%s\"", kind, version))
}

// CustomizeService ...
func CustomizeService(svc *corev1.Service, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	svc.Labels = ba.GetLabels()
	svc.Annotations = MergeMaps(svc.Annotations, ba.GetAnnotations())

	if len(svc.Spec.Ports) == 0 {
		svc.Spec.Ports = append(svc.Spec.Ports, corev1.ServicePort{})
	}

	svc.Spec.Ports[0].Port = ba.GetService().GetPort()
	svc.Spec.Ports[0].TargetPort = intstr.FromInt(int(ba.GetService().GetPort()))

	if ba.GetService().GetPortName() != "" {
		svc.Spec.Ports[0].Name = ba.GetService().GetPortName()
	} else {
		svc.Spec.Ports[0].Name = strconv.Itoa(int(ba.GetService().GetPort())) + "-tcp"
	}

	if *ba.GetService().GetType() == corev1.ServiceTypeNodePort && ba.GetService().GetNodePort() != nil {
		svc.Spec.Ports[0].NodePort = *ba.GetService().GetNodePort()
	}

	if *ba.GetService().GetType() == corev1.ServiceTypeClusterIP {
		svc.Spec.Ports[0].NodePort = 0
	}

	svc.Spec.Type = *ba.GetService().GetType()
	svc.Spec.Selector = map[string]string{
		"app.kubernetes.io/instance": obj.GetName(),
	}

	if ba.GetService().GetTargetPort() != nil {
		svc.Spec.Ports[0].TargetPort = intstr.FromInt(int(*ba.GetService().GetTargetPort()))
	}

	numOfAdditionalPorts := len(ba.GetService().GetPorts())
	numOfCurrentPorts := len(svc.Spec.Ports) - 1
	for i := 0; i < numOfAdditionalPorts; i++ {
		for numOfCurrentPorts < numOfAdditionalPorts {
			svc.Spec.Ports = append(svc.Spec.Ports, corev1.ServicePort{})
			numOfCurrentPorts++
		}
		for numOfCurrentPorts > numOfAdditionalPorts && len(svc.Spec.Ports) != 0 {
			svc.Spec.Ports = svc.Spec.Ports[:len(svc.Spec.Ports)-1]
			numOfCurrentPorts--
		}
		svc.Spec.Ports[i+1].Port = ba.GetService().GetPorts()[i].Port
		svc.Spec.Ports[i+1].TargetPort = intstr.FromInt(int(ba.GetService().GetPorts()[i].Port))

		if ba.GetService().GetPorts()[i].Name != "" {
			svc.Spec.Ports[i+1].Name = ba.GetService().GetPorts()[i].Name
		} else {
			svc.Spec.Ports[i+1].Name = strconv.Itoa(int(ba.GetService().GetPorts()[i].Port)) + "-tcp"
		}

		if ba.GetService().GetPorts()[i].TargetPort.String() != "" {
			svc.Spec.Ports[i+1].TargetPort = intstr.FromInt(ba.GetService().GetPorts()[i].TargetPort.IntValue())
		}

		if *ba.GetService().GetType() == corev1.ServiceTypeNodePort && ba.GetService().GetPorts()[i].NodePort != 0 {
			svc.Spec.Ports[i+1].NodePort = ba.GetService().GetPorts()[i].NodePort
		}

		if *ba.GetService().GetType() == corev1.ServiceTypeClusterIP {
			svc.Spec.Ports[i+1].NodePort = 0
		}
	}
	if len(ba.GetService().GetPorts()) == 0 {
		for numOfCurrentPorts > 0 {
			svc.Spec.Ports = svc.Spec.Ports[:len(svc.Spec.Ports)-1]
			numOfCurrentPorts--
		}
	}
}

// CustomizeServiceBindingSecret ...
func CustomizeServiceBindingSecret(secret *corev1.Secret, auth map[string]string, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	secret.Labels = ba.GetLabels()
	secret.Annotations = MergeMaps(secret.Annotations, ba.GetAnnotations())

	secretdata := map[string][]byte{}
	hostname := fmt.Sprintf("%s.%s.svc.cluster.local", obj.GetName(), obj.GetNamespace())
	secretdata["hostname"] = []byte(hostname)
	protocol := ba.GetService().GetProvides().GetProtocol()
	secretdata["protocol"] = []byte(protocol)
	url := fmt.Sprintf("%s://%s", protocol, hostname)
	if ba.GetCreateKnativeService() == nil || *(ba.GetCreateKnativeService()) == false {
		port := strconv.Itoa(int(ba.GetService().GetPort()))
		secretdata["port"] = []byte(port)
		url = fmt.Sprintf("%s:%s", url, port)
	}
	if ba.GetService().GetProvides().GetContext() != "" {
		context := strings.TrimPrefix(ba.GetService().GetProvides().GetContext(), "/")
		secretdata["context"] = []byte(context)
		url = fmt.Sprintf("%s/%s", url, context)
	}
	secretdata["url"] = []byte(url)
	if auth != nil {
		if username, ok := auth["username"]; ok {
			secretdata["username"] = []byte(username)
		}
		if password, ok := auth["password"]; ok {
			secretdata["password"] = []byte(password)
		}
	}

	secret.Data = secretdata
}

// CustomizeAffinity ...
func CustomizeAffinity(affinity *corev1.Affinity, ba common.BaseComponent) {

	archs := ba.GetArchitecture()

	if ba.GetAffinity() != nil {
		affinity.NodeAffinity = ba.GetAffinity().GetNodeAffinity()
		affinity.PodAffinity = ba.GetAffinity().GetPodAffinity()
		affinity.PodAntiAffinity = ba.GetAffinity().GetPodAntiAffinity()

		if len(archs) == 0 {
			archs = ba.GetAffinity().GetArchitecture()
		}

		if len(ba.GetAffinity().GetNodeAffinityLabels()) > 0 {
			if affinity.NodeAffinity == nil {
				affinity.NodeAffinity = &corev1.NodeAffinity{}
			}
			if affinity.NodeAffinity.RequiredDuringSchedulingIgnoredDuringExecution == nil {
				affinity.NodeAffinity.RequiredDuringSchedulingIgnoredDuringExecution = &corev1.NodeSelector{}
			}
			nodeSelector := affinity.NodeAffinity.RequiredDuringSchedulingIgnoredDuringExecution

			if len(nodeSelector.NodeSelectorTerms) == 0 {
				nodeSelector.NodeSelectorTerms = append(nodeSelector.NodeSelectorTerms, corev1.NodeSelectorTerm{})
			}
			labels := ba.GetAffinity().GetNodeAffinityLabels()

			keys := make([]string, 0, len(labels))
			for k := range labels {
				keys = append(keys, k)
			}
			sort.Strings(keys)

			for i := range nodeSelector.NodeSelectorTerms {

				for _, key := range keys {
					values := strings.Split(labels[key], ",")
					for i := range values {
						values[i] = strings.TrimSpace(values[i])
					}

					requirement := corev1.NodeSelectorRequirement{
						Key:      key,
						Operator: corev1.NodeSelectorOpIn,
						Values:   values,
					}

					nodeSelector.NodeSelectorTerms[i].MatchExpressions = append(nodeSelector.NodeSelectorTerms[i].MatchExpressions, requirement)
				}

			}
		}
	}

	if len(archs) > 0 {
		if affinity.NodeAffinity == nil {
			affinity.NodeAffinity = &corev1.NodeAffinity{}
		}
		if affinity.NodeAffinity.RequiredDuringSchedulingIgnoredDuringExecution == nil {
			affinity.NodeAffinity.RequiredDuringSchedulingIgnoredDuringExecution = &corev1.NodeSelector{}
		}

		nodeSelector := affinity.NodeAffinity.RequiredDuringSchedulingIgnoredDuringExecution

		if len(nodeSelector.NodeSelectorTerms) == 0 {
			nodeSelector.NodeSelectorTerms = append(nodeSelector.NodeSelectorTerms, corev1.NodeSelectorTerm{})
		}

		for i := range nodeSelector.NodeSelectorTerms {
			nodeSelector.NodeSelectorTerms[i].MatchExpressions = append(nodeSelector.NodeSelectorTerms[i].MatchExpressions,
				corev1.NodeSelectorRequirement{
					Key:      "kubernetes.io/arch",
					Operator: corev1.NodeSelectorOpIn,
					Values:   archs,
				},
			)
		}

		for i := range archs {
			term := corev1.PreferredSchedulingTerm{
				Weight: int32(len(archs)) - int32(i),
				Preference: corev1.NodeSelectorTerm{
					MatchExpressions: []corev1.NodeSelectorRequirement{
						{
							Key:      "kubernetes.io/arch",
							Operator: corev1.NodeSelectorOpIn,
							Values:   []string{archs[i]},
						},
					},
				},
			}
			affinity.NodeAffinity.PreferredDuringSchedulingIgnoredDuringExecution = append(affinity.NodeAffinity.PreferredDuringSchedulingIgnoredDuringExecution, term)
		}
	}

}

// CustomizePodSpec ...
func CustomizePodSpec(pts *corev1.PodTemplateSpec, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	pts.Labels = ba.GetLabels()
	pts.Annotations = MergeMaps(pts.Annotations, ba.GetAnnotations())

	var appContainer corev1.Container
	if len(pts.Spec.Containers) == 0 {
		appContainer = corev1.Container{}
	} else {
		appContainer = *GetAppContainer(pts.Spec.Containers)
	}

	appContainer.Name = "app"
	if len(appContainer.Ports) == 0 {
		appContainer.Ports = append(appContainer.Ports, corev1.ContainerPort{})
	}

	if ba.GetService().GetTargetPort() != nil {
		appContainer.Ports[0].ContainerPort = *ba.GetService().GetTargetPort()
	} else {
		appContainer.Ports[0].ContainerPort = ba.GetService().GetPort()
	}

	appContainer.Image = ba.GetStatus().GetImageReference()
	if ba.GetService().GetPortName() != "" {
		appContainer.Ports[0].Name = ba.GetService().GetPortName()
	} else {
		appContainer.Ports[0].Name = strconv.Itoa(int(appContainer.Ports[0].ContainerPort)) + "-tcp"
	}
	if ba.GetResourceConstraints() != nil {
		appContainer.Resources = *ba.GetResourceConstraints()
	}
	appContainer.ReadinessProbe = ba.GetReadinessProbe()
	appContainer.LivenessProbe = ba.GetLivenessProbe()

	if ba.GetPullPolicy() != nil {
		appContainer.ImagePullPolicy = *ba.GetPullPolicy()
	}
	appContainer.Command = ba.GetCommand()
	appContainer.Env = ba.GetEnv()
	appContainer.EnvFrom = ba.GetEnvFrom()

	pts.Spec.InitContainers = ba.GetInitContainers()

	appContainer.VolumeMounts = ba.GetVolumeMounts()
	pts.Spec.Volumes = ba.GetVolumes()

	if ba.GetService().GetCertificate() != nil || ba.GetService().GetCertificateSecretRef() != nil {
		secretName := obj.GetName() + "-svc-tls"
		if ba.GetService().GetCertificate() != nil && ba.GetService().GetCertificate().GetSpec().SecretName != "" {
			secretName = ba.GetService().GetCertificate().GetSpec().SecretName
		}
		if ba.GetService().GetCertificateSecretRef() != nil {
			secretName = *ba.GetService().GetCertificateSecretRef()
		}
		appContainer.Env = append(appContainer.Env, corev1.EnvVar{Name: "TLS_DIR", Value: "/etc/x509/certs"})
		pts.Spec.Volumes = append(pts.Spec.Volumes, corev1.Volume{
			Name: "svc-certificate",
			VolumeSource: corev1.VolumeSource{
				Secret: &corev1.SecretVolumeSource{
					SecretName: secretName,
				},
			},
		})
		appContainer.VolumeMounts = append(appContainer.VolumeMounts, corev1.VolumeMount{
			Name:      "svc-certificate",
			MountPath: "/etc/x509/certs",
			ReadOnly:  true,
		})
	}

	pts.Spec.Containers = append([]corev1.Container{appContainer}, ba.GetSidecarContainers()...)

	CustomizeConsumedServices(&pts.Spec, ba)

	if ba.GetServiceAccountName() != nil && *ba.GetServiceAccountName() != "" {
		pts.Spec.ServiceAccountName = *ba.GetServiceAccountName()
	} else {
		pts.Spec.ServiceAccountName = obj.GetName()
	}
	pts.Spec.RestartPolicy = corev1.RestartPolicyAlways
	pts.Spec.DNSPolicy = corev1.DNSClusterFirst

	if len(ba.GetArchitecture()) > 0 || ba.GetAffinity() != nil {
		pts.Spec.Affinity = &corev1.Affinity{}
		CustomizeAffinity(pts.Spec.Affinity, ba)
	} else {
		pts.Spec.Affinity = nil
	}
}

// CustomizeServiceBinding ...
func CustomizeServiceBinding(secret *corev1.Secret, podSpec *corev1.PodSpec, ba common.BaseComponent) {
	if len(ba.GetStatus().GetResolvedBindings()) != 0 {
		appContainer := GetAppContainer(podSpec.Containers)
		binding := ba.GetStatus().GetResolvedBindings()[0]
		bindingSecret := corev1.EnvFromSource{
			SecretRef: &corev1.SecretEnvSource{
				LocalObjectReference: corev1.LocalObjectReference{
					Name: binding,
				}},
		}
		appContainer.EnvFrom = append(appContainer.EnvFrom, bindingSecret)

		secretRev := corev1.EnvVar{
			Name:  "RESOLVED_BINDING_SECRET_REV",
			Value: secret.ResourceVersion}
		appContainer.Env = append(appContainer.Env, secretRev)
	}
}

// CustomizeConsumedServices ...
func CustomizeConsumedServices(podSpec *corev1.PodSpec, ba common.BaseComponent) {
	if ba.GetStatus().GetConsumedServices() != nil {
		appContainer := GetAppContainer(podSpec.Containers)
		for _, svc := range ba.GetStatus().GetConsumedServices()[common.ServiceBindingCategoryOpenAPI] {
			c, err := findConsumes(svc, ba)
			if err != nil {
				continue
			}
			if c.GetMountPath() != "" {
				actualMountPath := strings.Join([]string{c.GetMountPath(), c.GetNamespace(), c.GetName()}, "/")
				volMount := corev1.VolumeMount{Name: svc, MountPath: actualMountPath, ReadOnly: true}
				appContainer.VolumeMounts = append(appContainer.VolumeMounts, volMount)

				vol := corev1.Volume{
					Name: svc,
					VolumeSource: corev1.VolumeSource{
						Secret: &corev1.SecretVolumeSource{
							SecretName: svc,
						},
					},
				}
				podSpec.Volumes = append(podSpec.Volumes, vol)
			} else {
				// The characters allowed in names are: digits (0-9), lower case letters (a-z), -, and ..
				var keyPrefix string
				if c.GetNamespace() == "" {
					keyPrefix = normalizeEnvVariableName(c.GetName() + "_")
				} else {
					keyPrefix = normalizeEnvVariableName(c.GetNamespace() + "_" + c.GetName() + "_")
				}
				keys := []string{"username", "password", "url", "hostname", "protocol", "port", "context"}
				trueVal := true
				for _, k := range keys {
					env := corev1.EnvVar{
						Name: keyPrefix + strings.ToUpper(k),
						ValueFrom: &corev1.EnvVarSource{
							SecretKeyRef: &corev1.SecretKeySelector{
								LocalObjectReference: corev1.LocalObjectReference{
									Name: svc,
								},
								Key:      k,
								Optional: &trueVal,
							},
						},
					}
					appContainer.Env = append(appContainer.Env, env)
				}
			}
		}
	}
}

// CustomizePersistence ...
func CustomizePersistence(statefulSet *appsv1.StatefulSet, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	if len(statefulSet.Spec.VolumeClaimTemplates) == 0 {
		var pvc *corev1.PersistentVolumeClaim
		if ba.GetStorage().GetVolumeClaimTemplate() != nil {
			pvc = ba.GetStorage().GetVolumeClaimTemplate()
		} else {
			pvc = &corev1.PersistentVolumeClaim{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "pvc",
					Namespace: obj.GetNamespace(),
					Labels:    ba.GetLabels(),
				},
				Spec: corev1.PersistentVolumeClaimSpec{
					Resources: corev1.ResourceRequirements{
						Requests: corev1.ResourceList{
							corev1.ResourceStorage: resource.MustParse(ba.GetStorage().GetSize()),
						},
					},
					AccessModes: []corev1.PersistentVolumeAccessMode{
						corev1.ReadWriteOnce,
					},
				},
			}
			pvc.Annotations = MergeMaps(pvc.Annotations, ba.GetAnnotations())
		}
		statefulSet.Spec.VolumeClaimTemplates = append(statefulSet.Spec.VolumeClaimTemplates, *pvc)
	}

	appContainer := GetAppContainer(statefulSet.Spec.Template.Spec.Containers)

	if ba.GetStorage().GetMountPath() != "" {
		found := false
		for _, v := range appContainer.VolumeMounts {
			if v.Name == statefulSet.Spec.VolumeClaimTemplates[0].Name {
				found = true
			}
		}

		if !found {
			vm := corev1.VolumeMount{
				Name:      statefulSet.Spec.VolumeClaimTemplates[0].Name,
				MountPath: ba.GetStorage().GetMountPath(),
			}
			appContainer.VolumeMounts = append(appContainer.VolumeMounts, vm)
		}
	}

}

// CustomizeServiceAccount ...
func CustomizeServiceAccount(sa *corev1.ServiceAccount, ba common.BaseComponent) {
	sa.Labels = ba.GetLabels()
	sa.Annotations = MergeMaps(sa.Annotations, ba.GetAnnotations())

	if ba.GetPullSecret() != nil {
		if len(sa.ImagePullSecrets) == 0 {
			sa.ImagePullSecrets = append(sa.ImagePullSecrets, corev1.LocalObjectReference{
				Name: *ba.GetPullSecret(),
			})
		} else {
			sa.ImagePullSecrets[0].Name = *ba.GetPullSecret()
		}
	}
}

// CustomizeKnativeService ...
func CustomizeKnativeService(ksvc *servingv1alpha1.Service, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	ksvc.Labels = ba.GetLabels()
	ksvc.Annotations = MergeMaps(ksvc.Annotations, ba.GetAnnotations())

	// If `expose` is not set to `true`, make Knative route a private route by adding `serving.knative.dev/visibility: cluster-local`
	// to the Knative service. If `serving.knative.dev/visibility: XYZ` is defined in cr.Labels, `expose` always wins.
	if ba.GetExpose() != nil && *ba.GetExpose() {
		delete(ksvc.Labels, "serving.knative.dev/visibility")
	} else {
		ksvc.Labels["serving.knative.dev/visibility"] = "cluster-local"
	}

	if ksvc.Spec.Template == nil {
		ksvc.Spec.Template = &servingv1alpha1.RevisionTemplateSpec{}
	}
	if len(ksvc.Spec.Template.Spec.Containers) == 0 {
		ksvc.Spec.Template.Spec.Containers = append(ksvc.Spec.Template.Spec.Containers, corev1.Container{})
	}

	if len(ksvc.Spec.Template.Spec.Containers[0].Ports) == 0 {
		ksvc.Spec.Template.Spec.Containers[0].Ports = append(ksvc.Spec.Template.Spec.Containers[0].Ports, corev1.ContainerPort{})
	}
	ksvc.Spec.Template.ObjectMeta.Labels = ba.GetLabels()
	ksvc.Spec.Template.ObjectMeta.Annotations = MergeMaps(ksvc.Spec.Template.ObjectMeta.Annotations, ba.GetAnnotations())

	if ba.GetService().GetTargetPort() != nil {
		ksvc.Spec.Template.Spec.Containers[0].Ports[0].ContainerPort = *ba.GetService().GetTargetPort()
	} else {
		ksvc.Spec.Template.Spec.Containers[0].Ports[0].ContainerPort = ba.GetService().GetPort()
	}

	if ba.GetService().GetPortName() != "" {
		ksvc.Spec.Template.Spec.Containers[0].Ports[0].Name = ba.GetService().GetPortName()
	}

	ksvc.Spec.Template.Spec.Containers[0].Image = ba.GetStatus().GetImageReference()
	// Knative sets its own resource constraints
	//ksvc.Spec.Template.Spec.Containers[0].Resources = *cr.Spec.ResourceConstraints
	ksvc.Spec.Template.Spec.Containers[0].ReadinessProbe = ba.GetReadinessProbe()
	ksvc.Spec.Template.Spec.Containers[0].LivenessProbe = ba.GetLivenessProbe()
	ksvc.Spec.Template.Spec.Containers[0].ImagePullPolicy = *ba.GetPullPolicy()
	ksvc.Spec.Template.Spec.Containers[0].Env = ba.GetEnv()
	ksvc.Spec.Template.Spec.Containers[0].EnvFrom = ba.GetEnvFrom()

	ksvc.Spec.Template.Spec.Containers[0].VolumeMounts = ba.GetVolumeMounts()
	ksvc.Spec.Template.Spec.Volumes = ba.GetVolumes()
	CustomizeConsumedServices(&ksvc.Spec.Template.Spec.PodSpec, ba)

	if ba.GetServiceAccountName() != nil && *ba.GetServiceAccountName() != "" {
		ksvc.Spec.Template.Spec.ServiceAccountName = *ba.GetServiceAccountName()
	} else {
		ksvc.Spec.Template.Spec.ServiceAccountName = obj.GetName()
	}

	if ksvc.Spec.Template.Spec.Containers[0].LivenessProbe != nil {
		if ksvc.Spec.Template.Spec.Containers[0].LivenessProbe.HTTPGet != nil {
			ksvc.Spec.Template.Spec.Containers[0].LivenessProbe.HTTPGet.Port = intstr.IntOrString{}
		}
		if ksvc.Spec.Template.Spec.Containers[0].LivenessProbe.TCPSocket != nil {
			ksvc.Spec.Template.Spec.Containers[0].LivenessProbe.TCPSocket.Port = intstr.IntOrString{}
		}
	}

	if ksvc.Spec.Template.Spec.Containers[0].ReadinessProbe != nil {
		if ksvc.Spec.Template.Spec.Containers[0].ReadinessProbe.HTTPGet != nil {
			ksvc.Spec.Template.Spec.Containers[0].ReadinessProbe.HTTPGet.Port = intstr.IntOrString{}
		}
		if ksvc.Spec.Template.Spec.Containers[0].ReadinessProbe.TCPSocket != nil {
			ksvc.Spec.Template.Spec.Containers[0].ReadinessProbe.TCPSocket.Port = intstr.IntOrString{}
		}
	}
}

// CustomizeHPA ...
func CustomizeHPA(hpa *autoscalingv1.HorizontalPodAutoscaler, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	hpa.Labels = ba.GetLabels()
	hpa.Annotations = MergeMaps(hpa.Annotations, ba.GetAnnotations())

	hpa.Spec.MaxReplicas = ba.GetAutoscaling().GetMaxReplicas()
	hpa.Spec.MinReplicas = ba.GetAutoscaling().GetMinReplicas()
	hpa.Spec.TargetCPUUtilizationPercentage = ba.GetAutoscaling().GetTargetCPUUtilizationPercentage()

	hpa.Spec.ScaleTargetRef.Name = obj.GetName()
	hpa.Spec.ScaleTargetRef.APIVersion = "apps/v1"

	if ba.GetStorage() != nil {
		hpa.Spec.ScaleTargetRef.Kind = "StatefulSet"
	} else {
		hpa.Spec.ScaleTargetRef.Kind = "Deployment"
	}
}

// Validate if the BaseComponent is valid
func Validate(ba common.BaseComponent) (bool, error) {
	// Storage validation
	if ba.GetStorage() != nil {
		if ba.GetStorage().GetVolumeClaimTemplate() == nil {
			if ba.GetStorage().GetSize() == "" {
				return false, fmt.Errorf("validation failed: " + requiredFieldMessage("spec.storage.size"))
			}
			if _, err := resource.ParseQuantity(ba.GetStorage().GetSize()); err != nil {
				return false, fmt.Errorf("validation failed: cannot parse '%v': %v", ba.GetStorage().GetSize(), err)
			}
		}
	}

	return true, nil
}

func createValidationError(msg string) error {
	return fmt.Errorf("validation failed: " + msg)
}

func requiredFieldMessage(fieldPaths ...string) string {
	return "must set the field(s): " + strings.Join(fieldPaths, ", ")
}

// CustomizeServiceMonitor ...
func CustomizeServiceMonitor(sm *prometheusv1.ServiceMonitor, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	sm.Labels = ba.GetLabels()
	sm.Annotations = MergeMaps(sm.Annotations, ba.GetAnnotations())

	sm.Spec.Selector = metav1.LabelSelector{
		MatchLabels: map[string]string{
			"app.kubernetes.io/instance":                obj.GetName(),
			"monitor." + ba.GetGroupName() + "/enabled": "true",
		},
	}
	if len(sm.Spec.Endpoints) == 0 {
		sm.Spec.Endpoints = append(sm.Spec.Endpoints, prometheusv1.Endpoint{})
	}
	sm.Spec.Endpoints[0].Port = ""
	sm.Spec.Endpoints[0].TargetPort = nil
	if len(ba.GetMonitoring().GetEndpoints()) > 0 {
		port := ba.GetMonitoring().GetEndpoints()[0].Port
		targetPort := ba.GetMonitoring().GetEndpoints()[0].TargetPort
		if port != "" {
			sm.Spec.Endpoints[0].Port = port
		}
		if targetPort != nil {
			sm.Spec.Endpoints[0].TargetPort = targetPort
		}
		if port != "" && targetPort != nil {
			sm.Spec.Endpoints[0].TargetPort = nil
		}
	}
	if sm.Spec.Endpoints[0].Port == "" && sm.Spec.Endpoints[0].TargetPort == nil {
		if ba.GetService().GetPortName() != "" {
			sm.Spec.Endpoints[0].Port = ba.GetService().GetPortName()
		} else {
			sm.Spec.Endpoints[0].Port = strconv.Itoa(int(ba.GetService().GetPort())) + "-tcp"
		}
	}
	if len(ba.GetMonitoring().GetLabels()) > 0 {
		for k, v := range ba.GetMonitoring().GetLabels() {
			sm.Labels[k] = v
		}
	}

	if len(ba.GetMonitoring().GetEndpoints()) > 0 {
		endpoints := ba.GetMonitoring().GetEndpoints()
		if endpoints[0].Scheme != "" {
			sm.Spec.Endpoints[0].Scheme = endpoints[0].Scheme
		}
		if endpoints[0].Interval != "" {
			sm.Spec.Endpoints[0].Interval = endpoints[0].Interval
		}
		if endpoints[0].Path != "" {
			sm.Spec.Endpoints[0].Path = endpoints[0].Path
		}

		if endpoints[0].TLSConfig != nil {
			sm.Spec.Endpoints[0].TLSConfig = endpoints[0].TLSConfig
		}

		if endpoints[0].BasicAuth != nil {
			sm.Spec.Endpoints[0].BasicAuth = endpoints[0].BasicAuth
		}

		if endpoints[0].Params != nil {
			sm.Spec.Endpoints[0].Params = endpoints[0].Params
		}
		if endpoints[0].ScrapeTimeout != "" {
			sm.Spec.Endpoints[0].ScrapeTimeout = endpoints[0].ScrapeTimeout
		}
		if endpoints[0].BearerTokenFile != "" {
			sm.Spec.Endpoints[0].BearerTokenFile = endpoints[0].BearerTokenFile
		}
		sm.Spec.Endpoints[0].BearerTokenSecret = endpoints[0].BearerTokenSecret
		sm.Spec.Endpoints[0].ProxyURL = endpoints[0].ProxyURL
		sm.Spec.Endpoints[0].RelabelConfigs = endpoints[0].RelabelConfigs
		sm.Spec.Endpoints[0].MetricRelabelConfigs = endpoints[0].MetricRelabelConfigs
		sm.Spec.Endpoints[0].HonorTimestamps = endpoints[0].HonorTimestamps
		sm.Spec.Endpoints[0].HonorLabels = endpoints[0].HonorLabels

	}

}

// GetCondition ...
func GetCondition(conditionType openj9v1beta1.StatusConditionType, status *openj9v1beta1.RuntimeComponentStatus) *openj9v1beta1.StatusCondition {
	for i := range status.Conditions {
		if status.Conditions[i].Type == conditionType {
			return &status.Conditions[i]
		}
	}

	return nil
}

// SetCondition ...
func SetCondition(condition openj9v1beta1.StatusCondition, status *openj9v1beta1.RuntimeComponentStatus) {
	for i := range status.Conditions {
		if status.Conditions[i].Type == condition.Type {
			status.Conditions[i] = condition
			return
		}
	}

	status.Conditions = append(status.Conditions, condition)
}

// GetWatchNamespaces returns a slice of namespaces the operator should watch based on WATCH_NAMESPSCE value
// WATCH_NAMESPSCE value could be empty for watching the whole cluster or a comma-separated list of namespaces
func GetWatchNamespaces() ([]string, error) {
	watchNamespace, err := k8sutil.GetWatchNamespace()
	if err != nil {
		return nil, err
	}

	var watchNamespaces []string
	for _, ns := range strings.Split(watchNamespace, ",") {
		watchNamespaces = append(watchNamespaces, strings.TrimSpace(ns))
	}

	return watchNamespaces, nil
}

// MergeMaps returns a map containing the union of al the key-value pairs from the input maps. The order of the maps passed into the
// func, defines the importance. e.g. if (keyA, value1) is in map1, and (keyA, value2) is in map2, mergeMaps(map1, map2) would contain (keyA, value2).
// If the input map is nil, it is treated as empty map.
func MergeMaps(maps ...map[string]string) map[string]string {
	dest := make(map[string]string)

	for i := range maps {
		for key, value := range maps[i] {
			dest[key] = value
		}
	}

	return dest
}

// BuildServiceBindingSecretName returns secret name of a consumable service
func BuildServiceBindingSecretName(name, namespace string) string {
	return fmt.Sprintf("%s-%s", namespace, name)
}

func findConsumes(secretName string, ba common.BaseComponent) (common.ServiceBindingConsumes, error) {
	for _, v := range ba.GetService().GetConsumes() {
		namespace := ""
		if v.GetNamespace() == "" {
			namespace = ba.(metav1.Object).GetNamespace()
		} else {
			namespace = v.GetNamespace()
		}
		if BuildServiceBindingSecretName(v.GetName(), namespace) == secretName {
			return v, nil
		}
	}

	return nil, fmt.Errorf("Failed to find mountPath value")
}

// ContainsString returns true if `s` is in the slice. Otherwise, returns false
func ContainsString(slice []string, s string) bool {
	for _, str := range slice {
		if str == s {
			return true
		}
	}
	return false
}

// AppendIfNotSubstring appends `a` to comma-separated list of strings in `s`
func AppendIfNotSubstring(a, s string) string {
	if s == "" {
		return a
	}
	subs := strings.Split(s, ",")
	if !ContainsString(subs, a) {
		subs = append(subs, a)
	}
	return strings.Join(subs, ",")
}

// EnsureOwnerRef adds the ownerref if needed. Removes ownerrefs with conflicting UIDs.
// Returns true if the input is mutated. Copied from "https://github.com/openshift/library-go/blob/release-4.5/pkg/controller/ownerref.go"
func EnsureOwnerRef(metadata metav1.Object, newOwnerRef metav1.OwnerReference) bool {
	foundButNotEqual := false
	for _, existingOwnerRef := range metadata.GetOwnerReferences() {
		if existingOwnerRef.APIVersion == newOwnerRef.APIVersion &&
			existingOwnerRef.Kind == newOwnerRef.Kind &&
			existingOwnerRef.Name == newOwnerRef.Name {

			// if we're completely the same, there's nothing to do
			if equality.Semantic.DeepEqual(existingOwnerRef, newOwnerRef) {
				return false
			}

			foundButNotEqual = true
			break
		}
	}

	// if we weren't found, then we just need to add ourselves
	if !foundButNotEqual {
		metadata.SetOwnerReferences(append(metadata.GetOwnerReferences(), newOwnerRef))
		return true
	}

	// if we need to remove an existing ownerRef, just do the easy thing and build it back from scratch
	newOwnerRefs := []metav1.OwnerReference{newOwnerRef}
	for i := range metadata.GetOwnerReferences() {
		existingOwnerRef := metadata.GetOwnerReferences()[i]
		if existingOwnerRef.APIVersion == newOwnerRef.APIVersion &&
			existingOwnerRef.Kind == newOwnerRef.Kind &&
			existingOwnerRef.Name == newOwnerRef.Name {
			continue
		}
		newOwnerRefs = append(newOwnerRefs, existingOwnerRef)
	}
	metadata.SetOwnerReferences(newOwnerRefs)
	return true
}

func normalizeEnvVariableName(name string) string {
	return strings.NewReplacer("-", "_", ".", "_").Replace(strings.ToUpper(name))
}

// GetConnectToAnnotation returns a map containing a key-value annotatio. The key is `app.openshift.io/connects-to`.
// The value is a comma-seperated list of applications `ba` is connectiong to based on `spec.service.consumes`.
func GetConnectToAnnotation(ba common.BaseComponent) map[string]string {
	connectTo := []string{}
	for _, con := range ba.GetService().GetConsumes() {
		if ba.(metav1.Object).GetNamespace() == con.GetNamespace() {
			connectTo = append(connectTo, con.GetName())
		}
	}
	anno := map[string]string{}
	if len(connectTo) > 0 {
		anno["app.openshift.io/connects-to"] = strings.Join(connectTo, ",")
	}
	return anno
}

// GetOpenShiftAnnotations returns OpenShift specific annotations
func GetOpenShiftAnnotations(ba common.BaseComponent) map[string]string {
	// Conversion table between the pseudo Open Container Initiative <-> OpenShift annotations
	conversionMap := map[string]string{
		"image.opencontainers.org/source":   "app.openshift.io/vcs-uri",
		"image.opencontainers.org/revision": "app.openshift.io/vcs-ref",
	}

	annos := map[string]string{}
	for from, to := range conversionMap {
		if annoVal, ok := ba.GetAnnotations()[from]; ok {
			annos[to] = annoVal
		}
	}

	return MergeMaps(annos, GetConnectToAnnotation(ba))
}

// IsClusterWide returns true if watchNamespaces is set to [""]
func IsClusterWide(watchNamespaces []string) bool {
	return len(watchNamespaces) == 1 && watchNamespaces[0] == ""
}

// GetAppContainer returns the container that is running the app
func GetAppContainer(containerList []corev1.Container) *corev1.Container {
	for i := 0; i < len(containerList); i++ {
		if containerList[i].Name == "app" {
			return &containerList[i]
		}
	}
	return &containerList[0]
}

// CustomizeIngress customizes ingress resource
func CustomizeIngress(ing *networkingv1beta1.Ingress, ba common.BaseComponent) {
	obj := ba.(metav1.Object)
	ing.Labels = ba.GetLabels()
	servicePort := strconv.Itoa(int(ba.GetService().GetPort())) + "-tcp"
	host := ""
	path := ""
	rt := ba.GetRoute()
	if rt != nil {
		host = rt.GetHost()
		path = rt.GetPath()
		ing.Annotations = MergeMaps(ing.Annotations, ba.GetAnnotations(), rt.GetAnnotations())
	} else {
		ing.Annotations = MergeMaps(ing.Annotations, ba.GetAnnotations())
	}

	if ba.GetService().GetPortName() != "" {
		servicePort = ba.GetService().GetPortName()
	}

	if host == "" && common.Config[common.OpConfigDefaultHostname] != "" {
		host = obj.GetName() + "-" + obj.GetNamespace() + "." + common.Config[common.OpConfigDefaultHostname]
	}
	if host == "" {
		l := log.WithValues("Request.Namespace", obj.GetNamespace(), "Request.Name", obj.GetName())
		l.Info("No Ingress hostname is provided. Ingress might not function correctly without hostname. It is recommended to set Ingress host or to provide default value through operator's config map.")
	}

	ing.Spec.Rules = []networkingv1beta1.IngressRule{
		{
			Host: host,
			IngressRuleValue: networkingv1beta1.IngressRuleValue{
				HTTP: &networkingv1beta1.HTTPIngressRuleValue{
					Paths: []networkingv1beta1.HTTPIngressPath{
						{
							Path: path,
							Backend: networkingv1beta1.IngressBackend{
								ServiceName: obj.GetName(),
								ServicePort: intstr.IntOrString{Type: intstr.String, StrVal: servicePort},
							},
						},
					},
				},
			},
		},
	}

	tlsSecretName := ""
	if rt != nil && rt.GetCertificate() != nil {
		tlsSecretName = obj.GetName() + "-route-tls"
		if rt.GetCertificate().GetSpec().SecretName != "" {
			tlsSecretName = obj.GetName() + rt.GetCertificate().GetSpec().SecretName
		}
	}
	if rt != nil && rt.GetCertificateSecretRef() != nil && *rt.GetCertificateSecretRef() != "" {
		tlsSecretName = *rt.GetCertificateSecretRef()
	}
	if tlsSecretName != "" && host != "" {
		ing.Spec.TLS = []networkingv1beta1.IngressTLS{
			{
				Hosts:      []string{host},
				SecretName: tlsSecretName,
			},
		}
	} else {
		ing.Spec.TLS = nil
	}
}
